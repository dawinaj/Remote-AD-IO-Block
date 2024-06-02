#include "Board.h"

#include <limits>
#include <cmath>

#include <atomic>
#include <mutex>

#include <esp_timer.h>
#include <soc/gpio_reg.h>
#include <driver/gptimer.h>

#include "Communicator.h"

using Interpreter::OPCode;

#define SYNC_USE_NOTIF_NOT_SEM 1

namespace Board
{
	namespace
	{
		// SPECIFICATION
		constexpr size_t an_in_num = 4;
		constexpr size_t an_out_num = 2;

		constexpr size_t dg_in_num = 4;
		constexpr size_t dg_out_num = 4;

		// HARDWARE SETUP
		constexpr std::array<gpio_num_t, dg_in_num> dig_in = {GPIO_NUM_34, GPIO_NUM_35, GPIO_NUM_36, GPIO_NUM_39};
		constexpr std::array<gpio_num_t, dg_out_num> dig_out = {GPIO_NUM_4, GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_27};

		MCP23008 expander_a(I2C_NUM_0, 0b000);
		MCP23008 expander_b(I2C_NUM_0, 0b001);

		MCP3204 adc(SPI3_HOST, GPIO_NUM_5, 2'000'000);
		MCP4922 dac(SPI2_HOST, GPIO_NUM_15, 20'000'000);

		// STATE MACHINE
		uint32_t dg_out_state = 0;
		std::array<AnIn_Range, an_in_num> an_in_range;

		// CONFIG
		std::vector<Generator> generators;
		Interpreter::Program program;

		//================================//
		//            HELPERS             //
		//================================//

		// SOFTWARE SETUP
		TaskHandle_t execute_task = nullptr;
		std::mutex data_mutex;

		gptimer_handle_t sync_timer = nullptr;
		gptimer_alarm_config_t sync_alarm_cfg = {};

#if SYNC_USE_NOTIF_NOT_SEM
		constexpr UBaseType_t notif_idx = 0;
#else
		DRAM_ATTR SemaphoreHandle_t sync_semaphore;
#endif

		// EXECUTION
		uint64_t time_now = 0;
		uint64_t &time_sync = sync_alarm_cfg.alarm_count;
		DRAM_ATTR bool wait_for_sync = false;

		// ADC/DAC TRANSACTIONS
		std::array<spi_transaction_t, an_in_num> trx_in;
		std::array<spi_transaction_t, an_out_num> trx_out;

		// CONSTANTS
		constexpr uint32_t dg_out_mask = (1 << dg_out_num) - 1;
		constexpr uint32_t dg_in_mask = (1 << dg_in_num) - 1;

		// LUT
		constexpr size_t dg_out_lut_sz = 1 << dg_out_num;
		constexpr std::array<uint32_t, dg_out_lut_sz> dg_out_lut_s = []()
		{
			std::array<uint32_t, dg_out_lut_sz> ret = {};
			for (size_t in = 0; in < dg_out_lut_sz; ++in)
				for (size_t b = 0; b < dg_out_num; ++b)
					if (in & BIT(b))
						ret[in] |= BIT(dig_out[b]);
			return ret;
		}();
		constexpr std::array<uint32_t, dg_out_lut_sz> dg_out_lut_r = []()
		{
			std::array<uint32_t, dg_out_lut_sz> ret = {};
			for (size_t in = 0; in < dg_out_lut_sz; ++in)
				for (size_t b = 0; b < dg_out_num; ++b)
					if (~in & BIT(b))
						ret[in] |= BIT(dig_out[b]);
			return ret;
		}();

		// I/O conversion
		constexpr int32_t halfrangein = MCP3204::ref / 2;
		constexpr int32_t halfrangeout = MCP4922::ref / 2;
	}

	//================================//
	//          DECLARATIONS          //
	//================================//

	//================================//
	//         IMPLEMENTATION         //
	//================================//

	//----------------//
	//    HELPERS     //
	//----------------//

	static inline uint8_t range_to_expander_steering(AnIn_Range r)
	{
		static constexpr uint8_t steering[] = {0b0000, 0b0001, 0b0010, 0b0100};
		return steering[static_cast<uint8_t>(r)];
	}

	static inline constexpr int32_t adc_offset(MCP3204::out_t val)
	{
		return static_cast<int32_t>(val) - halfrangein;
	}

	static inline constexpr MCP4922::in_t dac_offset(int32_t val)
	{
		return val + halfrangeout;
	}

	template <typename num_t, int32_t mul = 1>
	static num_t bin_to_phy(Input in, int32_t sum, int32_t cnt)
	{
		constexpr num_t ratio = u_ref * mul / halfrangein;

		if (in == Input::None || in == Input::Inv) [[unlikely]]
			return 0;

		size_t inidx = static_cast<size_t>(in) - 1;
		size_t rngidx = static_cast<size_t>(an_in_range[inidx]);
		switch (in)
		{
		case Input::In1:
		case Input::In2:
		case Input::In3:
			return sum * ratio * volt_divs[rngidx] / cnt;
		case Input::In4:
			return sum * ratio / curr_gains[rngidx] / cnt * ItoU_input;
		default:
			return 0;
		}
	}

	static MCP4922::in_t phy_to_dac(Output out, float val)
	{
		constexpr float ratio = halfrangeout / out_ref;

		if (out == Output::None || out == Output::Inv) [[unlikely]]
			return 0;

		if (out == Output::Out2)
			val *= UtoI_output;

		if (val >= out_ref) [[unlikely]]
			return MCP4922::max;
		if (val <= -out_ref) [[unlikely]]
			return MCP4922::min;

		return dac_offset(std::round(val * ratio));
	}

	//----------------//
	//    BACKEND     //
	//----------------//

	// ANALOG INUPT

	static esp_err_t analog_input_range(Input in, AnIn_Range r)
	{
		if (in == Input::None || in >= Input::Inv) [[unlikely]]
			return ESP_ERR_INVALID_ARG;

		uint8_t pos = static_cast<uint8_t>(in) - 1;
		an_in_range[pos] = r;
		return ESP_OK;
	}

	static esp_err_t analog_inputs_disable()
	{
		ESP_RETURN_ON_ERROR(
			expander_a.set_pins(0x00),
			TAG, "Failed to expander_a.set_pins!");
		ESP_RETURN_ON_ERROR(
			expander_b.set_pins(0x00),
			TAG, "Failed to expander_a.set_pins!");

		return ESP_OK;
	}

	static esp_err_t analog_inputs_enable()
	{
		uint8_t lower = range_to_expander_steering(an_in_range[0]) | range_to_expander_steering(an_in_range[1]) << 4;
		uint8_t upper = range_to_expander_steering(an_in_range[2]) | range_to_expander_steering(an_in_range[3]) << 4;

		// ESP_LOGV(TAG, "AIEN: " BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(lower), BYTE_TO_BINARY(upper));

		ESP_RETURN_ON_ERROR(
			expander_a.set_pins(lower),
			TAG, "Failed to expander_a.set_pins!");

		ESP_RETURN_ON_ERROR(
			expander_b.set_pins(upper),
			TAG, "Failed to expander_b.set_pins!");

		return ESP_OK;
	}

	static MCP3204::out_t analog_input_read(Input in, MCP3204::out_t &out)
	{
		if (in == Input::None || in == Input::Inv) [[unlikely]]
			return ESP_ERR_INVALID_ARG;

		spi_transaction_t &trx = trx_in[static_cast<size_t>(in) - 1];

		ESP_RETURN_ON_ERROR(
			adc.send_trx(trx),
			TAG, "Failed to ADC send_trx!");

		ESP_RETURN_ON_ERROR(
			adc.recv_trx(),
			TAG, "Failed to ADC recv_trx!");

		out = adc.parse_trx(trx);

		return ESP_OK;
	}

	// ANALOG OUTPUT

	static esp_err_t analog_output_write(Output out, MCP4922::in_t val)
	{
		if (out == Output::None || out == Output::Inv) [[unlikely]]
			return ESP_ERR_INVALID_ARG;

		spi_transaction_t &trx = trx_out[static_cast<size_t>(out) - 1];
		dac.write_trx(trx, val);

		// ESP_LOGD(TAG, "Value to set: 0b" BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(val >> 8), BYTE_TO_BINARY(val));

		ESP_RETURN_ON_ERROR(
			dac.send_trx(trx),
			TAG, "Failed to dac.send_trx!");

		ESP_RETURN_ON_ERROR(
			dac.recv_trx(),
			TAG, "Failed to dac.recv_trx!");

		return ESP_OK;
	}

	static esp_err_t analog_outputs_reset()
	{
		constexpr MCP4922::in_t midpoint = MCP4922::ref / 2;
		ESP_RETURN_ON_ERROR(
			analog_output_write(Output::Out1, midpoint),
			TAG, "Failed to analog_output_write 1!");

		ESP_RETURN_ON_ERROR(
			analog_output_write(Output::Out2, midpoint),
			TAG, "Failed to analog_output_write 2!");

		return ESP_OK;
	}

	// DIGITAL OUTPUT

	static void digital_outputs_to_registers()
	{
		dg_out_state &= dg_out_mask;
		REG_WRITE(GPIO_OUT_W1TS_REG, dg_out_lut_s[dg_out_state]);
		REG_WRITE(GPIO_OUT_W1TC_REG, dg_out_lut_r[dg_out_state]);
	}

	static void digital_outputs_wr(uint32_t in)
	{
		dg_out_state = in;
		digital_outputs_to_registers();
	}
	static void digital_outputs_set(uint32_t in)
	{
		dg_out_state |= in;
		digital_outputs_to_registers();
	}
	static void digital_outputs_rst(uint32_t in)
	{
		dg_out_state &= ~in;
		digital_outputs_to_registers();
	}
	static void digital_outputs_and(uint32_t in)
	{
		dg_out_state &= in;
		digital_outputs_to_registers();
	}
	static void digital_outputs_xor(uint32_t in)
	{
		dg_out_state ^= in;
		digital_outputs_to_registers();
	}

	// DIGITAL INPUT

#pragma GCC push_options
#pragma GCC optimize("unroll-loops")
	static void digital_inputs_read(uint32_t &out)
	{
		uint32_t in = REG_READ(GPIO_IN1_REG);
		out = 0;
		for (size_t b = 0; b < dg_in_num; ++b)
			out |= !!(in & BIT(dig_in[b] - 32)) << b;
	}
#pragma GCC pop_options

	// HELPERS

	static esp_err_t port_cleanup()
	{
		for (size_t i = 0; i < an_in_num; ++i)
			an_in_range[i] = AnIn_Range::OFF;

		digital_outputs_wr(0);

		ESP_RETURN_ON_ERROR(
			analog_inputs_disable(),
			TAG, "Failed to analog_inputs_disable!");

		ESP_RETURN_ON_ERROR(
			analog_outputs_reset(),
			TAG, "Failed to analog_outputs_reset!");

		return ESP_OK;
	}

	// EXECUTABLE

	static IRAM_ATTR bool sync_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
	{
		BaseType_t high_task_awoken = pdFALSE;

		if (edata->alarm_value == time_sync)
#if SYNC_USE_NOTIF_NOT_SEM
			vTaskNotifyGiveIndexedFromISR(execute_task, notif_idx, &high_task_awoken);
#else
			xSemaphoreGiveFromISR(sync_semaphore, &high_task_awoken);
#endif
		// ESP_EARLY_LOGI("TMRISR", "/|\\");
		// return whether we need to yield at the end of ISR
		return high_task_awoken == pdTRUE;
	}

#if SYNC_USE_NOTIF_NOT_SEM

#define WAIT_FOR_SYNC                                                                   \
	do                                                                                  \
	{                                                                                   \
		if (wait_for_sync)                                                              \
			while (ulTaskNotifyTakeIndexed(notif_idx, pdTRUE, portMAX_DELAY) != pdTRUE) \
				;                                                                       \
	} while (0)

#define CLEAR_SYNC ulTaskNotifyTakeIndexed(notif_idx, pdTRUE, 0)

#else

#define WAIT_FOR_SYNC                                                       \
	do                                                                      \
	{                                                                       \
		if (wait_for_sync)                                                  \
			while (xSemaphoreTake(sync_semaphore, portMAX_DELAY) != pdTRUE) \
				;                                                           \
	} while (0)

#define CLEAR_SYNC xSemaphoreTake(sync_semaphore, 0)

#endif

	static inline uint64_t &get_now()
	{
		gptimer_get_raw_count(sync_timer, &time_now);
		return time_now;
	}

	static void interpreter_task(void *arg)
	{
		__attribute__((unused)) esp_err_t ret; // used in on_false macros

		Input in;
		Output out;

		ESP_LOGI(TAG, "Starting the Board executor...");
		while (true)
		{
			ESP_LOGI(TAG, "Waiting for task...");
			while (!Communicator::is_running())
				vTaskDelay(1);

			ESP_LOGI(TAG, "Dispatched...");

			// lock access
			std::lock_guard<std::mutex> lock(data_mutex);

			// prepare hardware
			ESP_GOTO_ON_ERROR(
				port_cleanup(),
				label_fail, TAG, "Failed to port_cleanup!");

			// prepare software
			ESP_GOTO_ON_FALSE(
				program.isValid(),
				ESP_ERR_INVALID_STATE, label_fail, TAG, "Program is invalid!");

			program.reset();

			// letsgooo
			time_now = 0;
			time_sync = -1;
			wait_for_sync = false;

			ESP_GOTO_ON_ERROR(
				gptimer_set_raw_count(sync_timer, time_now),
				label_fail, TAG, "Failed to gptimer_set_raw_count!");

			ESP_GOTO_ON_ERROR(
				gptimer_set_alarm_action(sync_timer, &sync_alarm_cfg),
				label_fail, TAG, "Failed to gptimer_set_alarm_action!");

			ESP_GOTO_ON_ERROR(
				gptimer_start(sync_timer),
				label_fail, TAG, "Failed to gptimer_start!");

			time_sync = 0;

			while (true)
			{
				bool comm_ok = true;

				Interpreter::InstrPtr stmt = program.getInstr();

				if (stmt == Interpreter::nullinstr) [[unlikely]]
					break;

				in = static_cast<Input>(stmt->port);
				out = static_cast<Output>(stmt->port);

				// ESP_LOGD(TAG, "OPCode: %" PRId32 ", argu: %" PRIu32 ", argf: %f, port: %" PRIu8, int32_t(stmt->opc), stmt->arg.u, stmt->arg.f, stmt->port);
				// ESP_LOGD(TAG, "Now: %" PRIu64 ", Wait: %" PRIu64, time_now, time_sync);

				switch (stmt->opc)
				{
				case OPCode::DELAY:
					time_sync += stmt->arg.u;
					wait_for_sync = true;
					CLEAR_SYNC;
					ESP_GOTO_ON_ERROR(
						gptimer_set_alarm_action(sync_timer, &sync_alarm_cfg),
						label_fail, TAG, "Failed to gptimer_set_alarm_action in OPCode::DELAY!");
					continue;

				case OPCode::GETTM:
					time_sync = std::max(time_sync, get_now());
					wait_for_sync = true;
					CLEAR_SYNC;
					ESP_GOTO_ON_ERROR(
						gptimer_set_alarm_action(sync_timer, &sync_alarm_cfg),
						label_fail, TAG, "Failed to gptimer_set_alarm_action in OPCode::GETTM!");
					continue;

				case OPCode::RSTTM:
					WAIT_FOR_SYNC;
					time_sync = -1;
					wait_for_sync = false;
					time_now = 0;
					CLEAR_SYNC;
					ESP_GOTO_ON_ERROR(
						gptimer_set_alarm_action(sync_timer, &sync_alarm_cfg),
						label_fail, TAG, "Failed to gptimer_set_alarm_action in OPCode::RSTTM!");
					time_sync = 0;
					ESP_GOTO_ON_ERROR(
						gptimer_set_raw_count(sync_timer, time_now),
						label_fail, TAG, "Failed to gptimer_set_raw_count in OPCode::RSTTM!");
					continue;

				case OPCode::DIRD:
				{
					WAIT_FOR_SYNC;
					uint32_t val;
					digital_inputs_read(val);
					comm_ok = Communicator::write_data(get_now(), val);
					break;
				}

				case OPCode::DOWR:
					WAIT_FOR_SYNC;
					digital_outputs_wr(stmt->arg.u);
					break;
				case OPCode::DOSET:
					WAIT_FOR_SYNC;
					digital_outputs_set(stmt->arg.u);
					break;
				case OPCode::DORST:
					WAIT_FOR_SYNC;
					digital_outputs_rst(stmt->arg.u);
					break;
				case OPCode::DOAND:
					WAIT_FOR_SYNC;
					digital_outputs_and(stmt->arg.u);
					break;
				case OPCode::DOXOR:
					WAIT_FOR_SYNC;
					digital_outputs_xor(stmt->arg.u);
					break;

				case OPCode::AIRDF:
				{
					WAIT_FOR_SYNC;
					int32_t sum = 0;
					MCP3204::out_t rd;
					for (size_t r = stmt->arg.u; r; --r)
					{
						ESP_GOTO_ON_ERROR(
							analog_input_read(in, rd),
							label_fail, TAG, "Failed to analog_input_read in OPCode::AIRDF!");
						sum += adc_offset(rd);
					}
					float val = bin_to_phy<float>(in, sum, stmt->arg.u);
					comm_ok = Communicator::write_data(get_now(), val);
					break;
				}
				case OPCode::AIRDM:
				{
					WAIT_FOR_SYNC;
					int32_t sum = 0;
					MCP3204::out_t rd;
					for (size_t r = stmt->arg.u; r; --r)
					{
						ESP_GOTO_ON_ERROR(
							analog_input_read(in, rd),
							label_fail, TAG, "Failed to analog_input_read in OPCode::AIRDF!");
						sum += adc_offset(rd);
					}
					int32_t val = bin_to_phy<int32_t, 1'000>(in, sum, stmt->arg.u);
					comm_ok = Communicator::write_data(get_now(), val);
					break;
				}
				case OPCode::AIRDU:
				{
					WAIT_FOR_SYNC;
					int32_t sum = 0;
					MCP3204::out_t rd;
					for (size_t r = stmt->arg.u; r; --r)
					{
						ESP_GOTO_ON_ERROR(
							analog_input_read(in, rd),
							label_fail, TAG, "Failed to analog_input_read in OPCode::AIRDF!");
						sum += adc_offset(rd);
					}
					int32_t val = bin_to_phy<int32_t, 1'000'000>(in, sum, stmt->arg.u);
					comm_ok = Communicator::write_data(get_now(), val);
					break;
				}

				case OPCode::AOVAL:
				{
					MCP4922::in_t outval = phy_to_dac(out, stmt->arg.f);
					WAIT_FOR_SYNC;
					ESP_GOTO_ON_ERROR(
						analog_output_write(out, outval),
						label_fail, TAG, "Failed to analog_output_write in OPCode::AOVAL!");
					break;
				}
				case OPCode::AOGEN:
				{
					MCP4922::in_t outval = phy_to_dac(out, (stmt->arg.u < generators.size()) ? generators[stmt->arg.u].get(time_sync) : 0);
					WAIT_FOR_SYNC;
					ESP_GOTO_ON_ERROR(
						analog_output_write(out, outval),
						label_fail, TAG, "Failed to analog_output_write in OPCode::AOGEN!");
					break;
				}

				case OPCode::AIEN:
					WAIT_FOR_SYNC;
					ESP_GOTO_ON_ERROR(
						analog_inputs_enable(),
						label_fail, TAG, "Failed to analog_inputs_enable in OPCode::AOVAL!");
					break;
				case OPCode::AIDIS:
					WAIT_FOR_SYNC;
					ESP_GOTO_ON_ERROR(
						analog_inputs_disable(),
						label_fail, TAG, "Failed to analog_inputs_disable in OPCode::AIDIS!");
					break;
				case OPCode::AIRNG:
					WAIT_FOR_SYNC;
					ESP_GOTO_ON_ERROR(
						analog_input_range(in, static_cast<AnIn_Range>(stmt->arg.u)),
						label_fail, TAG, "Failed to analog_input_range in OPCode::AIRNG!");
					break;

				case OPCode::NOP:
					ESP_GOTO_ON_FALSE(
						false, ESP_FAIL,
						label_fail, TAG, "Failed in OPCode::NOP!");
					break;
				case OPCode::INV:
					ESP_GOTO_ON_FALSE(
						false, ESP_FAIL,
						label_fail, TAG, "Failed in OPCode::INV!");
					break;
				}

				wait_for_sync = false;

				ESP_GOTO_ON_FALSE(
					comm_ok,
					ESP_ERR_NO_MEM, label_fail, TAG, "Communicator fail - no buffer space!");

				if (Communicator::should_exit()) [[unlikely]]
				{
					ESP_LOGW(TAG, "Communicator requests to exit!");
					goto label_fail;
				}
			}

			WAIT_FOR_SYNC;

		label_fail:
			gptimer_stop(sync_timer);

			port_cleanup();

			ESP_LOGI(TAG, "Execution took %" PRIu64 "us", get_now());
			ESP_LOGI(TAG, "Exiting...");
			Communicator::confirm_exit();
		}
		// never ends
	}

	//----------------//
	//    FRONTEND    //
	//----------------//

	// INIT

	esp_err_t init()
	{
		ESP_LOGI(TAG, "Initing Board...");

		// GPIO
		for (size_t i = 0; i < dg_in_num; ++i)
		{
			gpio_set_direction(dig_in[i], GPIO_MODE_INPUT);
			gpio_pulldown_en(dig_in[i]);
		}

		for (size_t i = 0; i < dg_out_num; ++i)
			gpio_set_direction(dig_out[i], GPIO_MODE_OUTPUT);

		// ADC/DAC
		for (size_t i = 0; i < an_in_num; ++i)
			trx_in[i] = MCP3204::make_trx(i);

		for (size_t i = 0; i < an_out_num; ++i)
			trx_out[i] = MCP4922::make_trx(an_out_num - i - 1); // reversed, because rotated chip

		ESP_RETURN_ON_ERROR(
			adc.init(),
			TAG, "Error in adc.init!");

		ESP_RETURN_ON_ERROR(
			dac.init(),
			TAG, "Error in dac.init!");

		ESP_RETURN_ON_ERROR(
			adc.acquire_spi(),
			TAG, "Error in adc.acquire_spi!");

		ESP_RETURN_ON_ERROR(
			dac.acquire_spi(),
			TAG, "Error in dac.acquire_spi!");

		// EXPANDER
		ESP_RETURN_ON_ERROR(
			expander_a.init(true),
			TAG, "Error in expander_a.init!");

		ESP_RETURN_ON_ERROR(
			expander_b.init(true),
			TAG, "Error in expander_b.init!");

		// CLEANUP BOARD
		ESP_RETURN_ON_ERROR(
			port_cleanup(),
			TAG, "Error in port_cleanup!");

		// TIMER
#if SYNC_USE_NOTIF_NOT_SEM
#else
		ESP_RETURN_ON_FALSE(
			(sync_semaphore = xSemaphoreCreateBinary()),
			ESP_ERR_NO_MEM, TAG, "Error in xSemaphoreCreateBinary!");
#endif

		constexpr gptimer_config_t timer_config = {
			.clk_src = GPTIMER_CLK_SRC_DEFAULT,
			.direction = GPTIMER_COUNT_UP,
			.resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
			.flags = {
				.intr_shared = false,
			},
		};

		sync_alarm_cfg.reload_count = 0;
		sync_alarm_cfg.alarm_count = 0;
		sync_alarm_cfg.flags.auto_reload_on_alarm = false;

		constexpr gptimer_event_callbacks_t evt_cb_cfg = {
			.on_alarm = sync_callback,
		};

		ESP_RETURN_ON_ERROR(
			gptimer_new_timer(&timer_config, &sync_timer),
			TAG, "Error in gptimer_new_timer!");

		ESP_RETURN_ON_ERROR(
			gptimer_set_alarm_action(sync_timer, &sync_alarm_cfg),
			TAG, "Error in gptimer_set_alarm_action!");

		ESP_RETURN_ON_ERROR(
			gptimer_register_event_callbacks(sync_timer, &evt_cb_cfg, nullptr),
			TAG, "Error in gptimer_register_event_callbacks!");

		ESP_RETURN_ON_ERROR(
			gptimer_enable(sync_timer),
			TAG, "Error in gptimer_enable!");

		// SPAWN EXE TASK
		ESP_RETURN_ON_FALSE(
			xTaskCreatePinnedToCore(interpreter_task, "BoardTask", BOARD_MEM, nullptr, BOARD_PRT, &execute_task, CPU1),
			ESP_ERR_NO_MEM, TAG, "Error in xTaskCreatePinnedToCore!");

		ESP_LOGI(TAG, "Done!");
		return ESP_OK;
	}

	esp_err_t deinit()
	{
		ESP_LOGI(TAG, "Deiniting Board...");

		// KILL EXE TASK
		vTaskDelete(execute_task); // void
		execute_task = nullptr;

		// KILL TIMER
		ESP_RETURN_ON_ERROR(
			gptimer_disable(sync_timer),
			TAG, "Error in gptimer_disable!");

		ESP_RETURN_ON_ERROR(
			gptimer_del_timer(sync_timer),
			TAG, "Error in gptimer_del_timer!");

#if SYNC_USE_NOTIF_NOT_SEM
#else
		vSemaphoreDelete(sync_semaphore); // void
		sync_semaphore = nullptr;
#endif

		// CLEANUP BOARD
		ESP_RETURN_ON_ERROR(
			port_cleanup(),
			TAG, "Error in port_cleanup!");

		// EXPANDER
		ESP_RETURN_ON_ERROR(
			expander_a.deinit(),
			TAG, "Error in expander_a.deinit!");

		ESP_RETURN_ON_ERROR(
			expander_b.deinit(),
			TAG, "Error in expander_b.deinit!");

		// ADC/DAC
		ESP_RETURN_ON_ERROR(
			adc.release_spi(),
			TAG, "Error in adc.release_spi!");

		ESP_RETURN_ON_ERROR(
			dac.release_spi(),
			TAG, "Error in dac.release_spi!");

		ESP_RETURN_ON_ERROR(
			adc.deinit(),
			TAG, "Error in adc.deinit!");

		ESP_RETURN_ON_ERROR(
			dac.deinit(),
			TAG, "Error in dac.deinit!");

		ESP_LOGI(TAG, "Done!");
		return ESP_OK;
	}

	// INTERFACE

	esp_err_t move_config(Interpreter::Program &p, std::vector<Generator> &g)
	{
		if (!data_mutex.try_lock())
			return ESP_ERR_INVALID_STATE;

		program = std::move(p);
		generators = std::move(g);

		data_mutex.unlock();
		return ESP_OK;
	}

	esp_err_t give_sem_emergency()
	{
#if SYNC_USE_NOTIF_NOT_SEM
		if (xTaskNotifyGiveIndexed(execute_task, notif_idx) != pdTRUE)
#else
		if (xSemaphoreGive(sync_semaphore) != pdTRUE)
#endif
			return ESP_FAIL;
		return ESP_OK;
	}

	esp_err_t test()
	{
		return ESP_OK;
	}
}