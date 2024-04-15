#include "Board.h"

#include <limits>
#include <cmath>

#include <atomic>
#include <mutex>

#include <esp_timer.h>
#include <soc/gpio_reg.h>
#include <driver/gptimer.h>

#include "Communicator.h"

using Interpreter::Command;

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
		DRAM_ATTR SemaphoreHandle_t sync_semaphore;

		// EXECUTION
		uint64_t time_now = 0;
		uint64_t &waitfor = sync_alarm_cfg.alarm_count;
		bool wait_for_sem = true;

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

	template <typename num_t, int32_t mul = 1>
	static num_t adc_to_value(Input in, MCP3204::out_t val)
	{
		// Divider settings:       Min: 1V=>1V, Med: 10V=>1V, Max: 100V=>1V
		// Gains settings: R=1Ohm; Min: 1mA=>1V, Med: 10mA=>1V, Max: 100mA=>1V
		constexpr int32_t volt_divs[4] = {0, 1, 10, 100};	  // ratios of dividers | min range => min attn
		constexpr int32_t curr_gains[4] = {5, 1000, 100, 10}; // gains of instr.amp | min range => max gain

		// According to docs, Dout = 4096 * Uin/Uref. Assuming Uref=4096mV, Uin=Dout.
		// Important to note, values are !stretched! and shifted!. 2048 = 0V, 0 = -4096mV, 4096 = 4096mV

		constexpr int32_t offset = MCP3204::ref / 2;

		constexpr num_t ratio = u_ref * mul / offset;

		int32_t bipolar = val - offset;

		switch (in)
		{
		case Input::In1:
			return bipolar * ratio * volt_divs[static_cast<size_t>(an_in_range[0])];
		case Input::In2:
			return bipolar * ratio * volt_divs[static_cast<size_t>(an_in_range[1])];
		case Input::In3:
			return bipolar * ratio * volt_divs[static_cast<size_t>(an_in_range[2])];
		case Input::In4:
			return bipolar * ratio / curr_gains[static_cast<size_t>(an_in_range[3])] / ccvs_transresistance;
		default:
			return 0;
		}
	}

	static MCP4922::in_t value_to_dac(Output out, float val)
	{
		constexpr float maxabs = u_ref / 2 / 2 * 10; // bipolar and halved, out= -1V -- 1V, *10
		constexpr int32_t offset = MCP4922::ref / 2;
		constexpr float ratio = offset / maxabs;

		if (out == Output::None || out == Output::Inv) [[unlikely]]
			return 0;

		if (out == Output::Out2)
			val /= vccs_transconductance;

		if (val >= maxabs) [[unlikely]]
			return MCP4922::max;
		if (val <= -maxabs) [[unlikely]]
			return MCP4922::min;

		return static_cast<int32_t>(std::round(val * ratio)) + offset;
	}

	//----------------//
	//    BACKEND     //
	//----------------//

	// ANALOG INUPT

	static esp_err_t analog_input_range(Input in, AnIn_Range r)
	{
		if (in == Input::None || in == Input::Inv) [[unlikely]]
			return ESP_ERR_INVALID_ARG;

		uint8_t pos = static_cast<uint8_t>(in) - 1;
		an_in_range[pos] = r;
		return ESP_OK;
	}

	static esp_err_t analog_inputs_disable()
	{
		ESP_RETURN_ON_ERROR(
			expander_a.set_pins(0x00),
			TAG, "Failed to reset relays on a!");
		ESP_RETURN_ON_ERROR(
			expander_b.set_pins(0x00),
			TAG, "Failed to reset relays on b!");

		return ESP_OK;
	}

	static esp_err_t analog_inputs_enable()
	{
		uint8_t lower = range_to_expander_steering(an_in_range[0]) | range_to_expander_steering(an_in_range[1]) << 4;
		uint8_t upper = range_to_expander_steering(an_in_range[2]) | range_to_expander_steering(an_in_range[3]) << 4;

		ESP_RETURN_ON_ERROR(
			expander_a.set_pins(lower),
			TAG, "Failed to set relays on a!");

		ESP_RETURN_ON_ERROR(
			expander_b.set_pins(upper),
			TAG, "Failed to set relays on b!");

		return ESP_OK;
	}

	static esp_err_t analog_input_read(Input in, MCP3204::out_t &out)
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
		xSemaphoreGiveFromISR(sync_semaphore, &high_task_awoken);
		// return whether we need to yield at the end of ISR
		return high_task_awoken == pdTRUE;
	}

#define WAIT_FOR_SYNC                                                       \
	do                                                                      \
	{                                                                       \
		if (wait_for_sem)                                                   \
			while (xSemaphoreTake(sync_semaphore, portMAX_DELAY) != pdTRUE) \
				;                                                           \
	} while (0)

	static inline uint64_t &get_now()
	{
		gptimer_get_raw_count(sync_timer, &time_now);
		return time_now;
	}

	static inline esp_err_t clear_now()
	{
		time_now = 0;
		waitfor = 0;
		wait_for_sem = false;

		ESP_RETURN_ON_ERROR(
			gptimer_stop(sync_timer),
			TAG, "Failed to gptimer_stop!");

		ESP_RETURN_ON_ERROR(
			gptimer_set_raw_count(sync_timer, 0),
			TAG, "Failed to gptimer_set_raw_count!");

		ESP_RETURN_ON_ERROR(
			gptimer_set_alarm_action(sync_timer, &sync_alarm_cfg),
			TAG, "Failed to gptimer_set_alarm_action!");

		ESP_RETURN_ON_ERROR(
			gptimer_start(sync_timer),
			TAG, "Failed to gptimer_start!");

		return ESP_OK;
	}

	static void execute(void *arg)
	{
		__attribute__((unused)) esp_err_t ret; // used in on_false macros

		ESP_LOGI(TAG, "Starting the Board executor...");
		while (true)
		{
			ESP_LOGI(TAG, "Waiting for task...");
			while (!Communicator::check_if_running())
				vTaskDelay(1);

			ESP_LOGI(TAG, "Dispatched...");

			std::lock_guard<std::mutex> lock(data_mutex);

			Input in;
			Output out;

			ESP_GOTO_ON_ERROR(
				port_cleanup(),
				label_fail, TAG, "Failed to port_cleanup!");

			ESP_GOTO_ON_ERROR(
				clear_now(),
				label_fail, TAG, "Failed to clear_now!");

			program.reset();

			while (true)
			{
				bool comm_ok = true;

				Interpreter::StmtPtr stmt = program.getStmt();

				if (stmt == Interpreter::nullstmt) [[unlikely]]
					break;

				in = static_cast<Input>(stmt->port);
				out = static_cast<Output>(stmt->port);

				switch (stmt->cmd)
				{
				case Command::DELAY:
					waitfor += stmt->arg.u;
					ESP_GOTO_ON_ERROR(
						gptimer_set_alarm_action(sync_timer, &sync_alarm_cfg),
						label_fail, TAG, "Failed to gptimer_set_alarm_action in Command::DELAY!");
					wait_for_sem = (waitfor > get_now());
					if (!wait_for_sem)
						xSemaphoreTake(sync_semaphore, 0); // take in case of timer desync
					goto label_nosync;
					break;

				case Command::GETTM:
					get_now();
					if (time_now >= waitfor)
					{
						waitfor = time_now;
						wait_for_sem = false;
						xSemaphoreTake(sync_semaphore, 0); // take in case of timer desync
					}
					goto label_nosync;
					break;

				case Command::DIRD:
				{
					WAIT_FOR_SYNC;
					uint32_t val;
					digital_inputs_read(val);
					comm_ok = Communicator::write_data(get_now(), val);
					break;
				}

				case Command::DOWR:
					WAIT_FOR_SYNC;
					digital_outputs_wr(stmt->arg.u);
					break;
				case Command::DOSET:
					WAIT_FOR_SYNC;
					digital_outputs_set(stmt->arg.u);
					break;
				case Command::DORST:
					WAIT_FOR_SYNC;
					digital_outputs_rst(stmt->arg.u);
					break;
				case Command::DOAND:
					WAIT_FOR_SYNC;
					digital_outputs_and(stmt->arg.u);
					break;
				case Command::DOXOR:
					WAIT_FOR_SYNC;
					digital_outputs_xor(stmt->arg.u);
					break;

				case Command::AIRDF:
				{
					WAIT_FOR_SYNC;
					MCP3204::out_t rd;
					ESP_GOTO_ON_ERROR(
						analog_input_read(in, rd),
						label_fail, TAG, "Failed to analog_input_read in Command::AIRDF!");
					float val = adc_to_value<float>(in, rd);
					comm_ok = Communicator::write_data(get_now(), val);
					break;
				}
				case Command::AIRDM:
				{
					WAIT_FOR_SYNC;
					MCP3204::out_t rd;
					ESP_GOTO_ON_ERROR(
						analog_input_read(in, rd),
						label_fail, TAG, "Failed to analog_input_read in Command::AIRDM!");
					int32_t val = adc_to_value<int32_t, 1'000>(in, rd);
					comm_ok = Communicator::write_data(get_now(), val);
					break;
				}
				case Command::AIRDU:
				{
					WAIT_FOR_SYNC;
					MCP3204::out_t rd;
					ESP_GOTO_ON_ERROR(
						analog_input_read(in, rd),
						label_fail, TAG, "Failed to analog_input_read in Command::AIRDU!");
					int32_t val = adc_to_value<int32_t, 1'000'000>(in, rd);
					comm_ok = Communicator::write_data(get_now(), val);
					break;
				}

				case Command::AOVAL:
				{
					MCP4922::in_t outval = value_to_dac(out, stmt->arg.f);
					WAIT_FOR_SYNC;
					ESP_GOTO_ON_ERROR(
						analog_output_write(out, outval),
						label_fail, TAG, "Failed to analog_output_write in Command::AOVAL!");
					break;
				}
				case Command::AOGEN:
				{
					MCP4922::in_t outval = value_to_dac(out, (stmt->arg.u < generators.size()) ? generators[stmt->arg.u].get(waitfor) : 0);
					WAIT_FOR_SYNC;
					ESP_GOTO_ON_ERROR(
						analog_output_write(out, outval),
						label_fail, TAG, "Failed to analog_output_write in Command::AOGEN!");
					break;
				}

				case Command::AIEN:
					WAIT_FOR_SYNC;
					ESP_GOTO_ON_ERROR(
						analog_inputs_enable(),
						label_fail, TAG, "Failed to analog_inputs_enable in Command::AOVAL!");
					break;
				case Command::AIDIS:
					WAIT_FOR_SYNC;
					ESP_GOTO_ON_ERROR(
						analog_inputs_disable(),
						label_fail, TAG, "Failed to analog_inputs_disable in Command::AIDIS!");
					break;
				case Command::AIRNG:
					WAIT_FOR_SYNC;
					ESP_GOTO_ON_ERROR(
						analog_input_range(in, static_cast<AnIn_Range>(stmt->arg.u)),
						label_fail, TAG, "Failed to analog_input_range in Command::AIRNG!");
					break;

				case Command::NOP:
					ESP_GOTO_ON_FALSE(
						false, ESP_FAIL,
						label_fail, TAG, "Failed in Command::NOP!");
					break;
				}

				wait_for_sem = false;

			label_nosync:

				ESP_GOTO_ON_FALSE(
					comm_ok,
					ESP_ERR_NO_MEM, label_fail, TAG, "Communicator fail - no buffer space!");

				ESP_GOTO_ON_FALSE(
					!Communicator::check_if_should_exit(),
					ESP_FAIL, label_fail, TAG, "Communicator requests to exit!");
			}

			WAIT_FOR_SYNC;

		label_fail:
			port_cleanup();

			gptimer_stop(sync_timer);

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
		ESP_RETURN_ON_ERROR(
			adc.init(),
			TAG, "Error in adc.init!");

		ESP_RETURN_ON_ERROR(
			dac.init(),
			TAG, "Error in dac.init!");

		for (size_t i = 0; i < an_in_num; ++i)
			trx_in[i] = adc.make_trx(i);

		for (size_t i = 0; i < an_out_num; ++i)
			trx_out[i] = dac.make_trx(an_out_num - i - 1); // reversed, because rotated chip

		ESP_RETURN_ON_ERROR(
			adc.acquire_spi(),
			TAG, "Error in adc.acquire_spi!");

		ESP_RETURN_ON_ERROR(
			dac.acquire_spi(),
			TAG, "Error in dac.acquire_spi!");

		// CLEANUP BOARD
		ESP_RETURN_ON_ERROR(
			port_cleanup(),
			TAG, "Error in port_cleanup!");

		// TIMER
		ESP_RETURN_ON_FALSE(
			(sync_semaphore = xSemaphoreCreateBinary()),
			ESP_ERR_NO_MEM, TAG, "Error in xSemaphoreCreateBinary!");

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
			xTaskCreatePinnedToCore(execute, "BoardTask", BOARD_MEM, nullptr, BOARD_PRT, &execute_task, CPU1),
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

		vSemaphoreDelete(sync_semaphore); // void
		sync_semaphore = nullptr;

		// CLEANUP BOARD
		ESP_RETURN_ON_ERROR(
			port_cleanup(),
			TAG, "Error in port_cleanup!");

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
}