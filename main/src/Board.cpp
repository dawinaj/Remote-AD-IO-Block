#include "Board.h"

#include <limits>
#include <cmath>

#include <esp_log.h>
#include <esp_check.h>

#include <esp_timer.h>

#include <soc/gpio_reg.h>

#define NOOP __asm__ __volatile__("nop")

namespace Board
{
	constexpr size_t an_in_num = 4;
	constexpr size_t an_out_num = 2;

	constexpr size_t dg_in_num = 4;
	constexpr size_t dg_out_num = 4;

	constexpr std::array<gpio_num_t, dg_in_num> dig_in = {GPIO_NUM_34, GPIO_NUM_35, GPIO_NUM_36, GPIO_NUM_39};
	constexpr std::array<gpio_num_t, dg_out_num> dig_out = {GPIO_NUM_4, GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_27};

	MCP23008 expander_a(I2C_NUM_0, 0b000);
	MCP23008 expander_b(I2C_NUM_0, 0b001);

	MCP3204 adc(SPI3_HOST, GPIO_NUM_5, 2'000'000);
	MCP4922 dac(SPI2_HOST, GPIO_NUM_15, 20'000'000);

	std::array<AnIn_Range, an_in_num> an_in_range;

	std::array<spi_transaction_t, an_in_num> trx_in;
	std::array<spi_transaction_t, an_out_num> trx_out;

	std::vector<Generator> generators;
	Executing::Program program;

	// LUT
	constexpr uint32_t dg_out_mask = (1 << dg_out_num) - 1;
	constexpr uint32_t dg_in_mask = (1 << dg_in_num) - 1;

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

	void reset_outputs();
	inline uint8_t range_to_expander_steering(AnIn_Range);

	//

	esp_err_t init()
	{
		for (size_t i = 0; i < an_in_num; ++i)
			an_in_range[i] = AnIn_Range::OFF;

		for (size_t i = 0; i < an_in_num; ++i)
			trx_in[i] = adc.make_trx(i);

		for (size_t i = 0; i < an_out_num; ++i)
			trx_out[i] = adc.make_trx(an_out_num - i - 1); // reversed, because reversed chip

		for (size_t i = 0; i < dg_in_num; ++i)
			gpio_set_direction(dig_in[i], GPIO_MODE_INPUT);

		for (size_t i = 0; i < dg_out_num; ++i)
			gpio_set_direction(dig_out[i], GPIO_MODE_OUTPUT);

		// execute

		expander_a.set_pins(0x00);
		expander_b.set_pins(0x00);

		adc.acquire_spi();
		dac.acquire_spi();

		reset_outputs();

		ESP_LOGI(TAG, "Constructed!");

		return ESP_OK;
	}

	esp_err_t deinit()
	{
		adc.release_spi();
		dac.release_spi();

		expander_a.set_pins(0x00);
		expander_b.set_pins(0x00);

		ESP_LOGI(TAG, "Destructed!");

		return ESP_OK;
	}

	//

	esp_err_t set_input_range(Input in, AnIn_Range r)
	{
		if (in == Input::None || in == Input::Inv) [[unlikely]]
			return ESP_ERR_INVALID_ARG;

		uint8_t pos = static_cast<uint8_t>(in) - 1;
		an_in_range[pos] = r;
		return ESP_OK;
	}

	esp_err_t disable_inputs()
	{
		esp_err_t ret = ESP_OK;

		ESP_GOTO_ON_ERROR(
			expander_a.set_pins(0x00),
			err, TAG, "Failed to reset relays on a!");
		ESP_GOTO_ON_ERROR(
			expander_b.set_pins(0x00),
			err, TAG, "Failed to reset relays on b!");

		return ESP_OK;

	err:
		ESP_LOGE(TAG, "Failed to reset input relays! Error: %s", esp_err_to_name(ret));
		return ret;
	}

	esp_err_t enable_inputs()
	{
		esp_err_t ret = ESP_OK;

		uint8_t lower = range_to_expander_steering(an_in_range[0]) | range_to_expander_steering(an_in_range[1]) << 4;
		uint8_t upper = range_to_expander_steering(an_in_range[2]) | range_to_expander_steering(an_in_range[3]) << 4;

		ESP_GOTO_ON_ERROR(
			expander_a.set_pins(lower),
			err, TAG, "Failed to set relays on a!");
		ESP_GOTO_ON_ERROR(
			expander_b.set_pins(upper),
			err, TAG, "Failed to set relays on b!");

		return ESP_OK;

	err:
		ESP_LOGE(TAG, "Failed to set input relays! Error: %s", esp_err_to_name(ret));
		return ret;
	}

	void move_config(Executing::Program &p, std::vector<Generator> &g)
	{
		program = std::move(p);
		generators = std::move(g);
	}

	// general execution plan:
	// wait for trigger
	// in loop:
	// send input
	// read dig in
	// write output
	// write output
	// write dig out
	// recv input
	esp_err_t execute(WriteCb &&callback, std::atomic_bool &exit)
	{
		spi_transaction_t *trx1 = nullptr;
		spi_transaction_t *trx2 = nullptr;
		Generator *gen1 = nullptr;
		Generator *gen2 = nullptr;

		int64_t start = esp_timer_get_time();
		int64_t now = start;
		int64_t next = now;

		size_t iter = 0;

		goto exit;
		//
		/*/
			size_t in_idx = 0;
			size_t rpt = 0;

			Input in = Input::None;
			spi_transaction_t *trxin = nullptr;

			start = next; // reset deltatime

			float gen1_val = gen1->get(0);
			float gen2_val = gen2->get(0);

			bool dig_out[4] = {true, true, true, true};
			int64_t dig_next[4] = {start, start, start, start};
			size_t dig_idx[4] = {0, 0, 0, 0};

			size_t iter = 0;
			while (iter < general.sample_count)
			{
				// setup

				int16_t meas = 0;

				if (exec.do_anlg_inp)
					in = input.port_order[in_idx];

				if (exec.do_dgtl_out)
					for (size_t i = 0; i < 4; ++i)
						if (!output.dig_delays[i].empty())
						{
							while (now >= dig_next[i])
							{
								dig_out[i] = !dig_out[i];
								dig_next[i] += output.dig_delays[i][dig_idx[i]];
								if (++dig_idx[i] == output.dig_delays[i].size())
									dig_idx[i] = 0;
							}
						}

				// wait for synchronisation

				while ((now = esp_timer_get_time()) < next)
					NOOP;
				next = now + general.period_us;

				// lessago

				if (in != Input::None)
				{
					trxin = &trx_in[static_cast<uint8_t>(in) - 1];
					adc.send_trx(*trxin);
				}

				if (exec.do_dgtl_inp)
					meas = read_digital();

				if (exec.do_dgtl_out)
					write_digital(dig_out[0], dig_out[1], dig_out[2], dig_out[3]);

				if (exec.do_anlg_out)
				{
					if (!gen1->empty())
					{
						dac.write_trx(*trx1, conv_gen(volt_to_generated(gen1_val)));
						dac.send_trx(*trx1);
						gen1_val = gen1->get(next - start);
						dac.recv_trx();
					}
					if (!gen2->empty())
					{
						dac.write_trx(*trx2, conv_gen(volt_to_generated(gen2_val)));
						dac.send_trx(*trx2);
						gen2_val = gen2->get(next - start);
						dac.recv_trx();
					}
				}

				if (in != Input::None)
				{
					adc.recv_trx();
					meas |= conv_meas(adc.parse_trx(*trxin));
				}

				if (exec.do_dgtl_inp || exec.do_anlg_inp)
				{
					bool ok = callback(meas);

					if (!ok)
						goto exit;
				}

				if (exec.do_anlg_inp)
					if (++rpt == input.repeats)
					{
						rpt = 0;
						if (++in_idx == input.port_order.size())
							in_idx = 0;
					}

				++iter;
			}

		//*/

		// if (buf_pos)
		// 	input.callback(buffers[buf_idx].data(), buf_pos, ctx);

	exit:

		while ((now = esp_timer_get_time()) < next)
			NOOP;

		write_digital(0b0000);
		reset_outputs();

		ESP_LOGI(TAG, "Operation: %d cycles, took %d us, equals %f us/c", int(iter), int(next - start), float(next - start) / iter);

		return ESP_OK;
	}

	//

	/*/
	AnIn_Range range_decode(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
					   { return std::toupper(c); });
		if (s == "MIN")
			return AnIn_Range::Min;
		if (s == "MID")
			return AnIn_Range::Med;
		if (s == "MAX")
			return AnIn_Range::Max;
		return AnIn_Range::OFF;
	}
	//*/

	void reset_outputs()
	{
		dac.write_trx(trx_out[0], (MCP4922::max + 1) / 2);
		dac.write_trx(trx_out[1], (MCP4922::max + 1) / 2);
		dac.send_trx(trx_out[0]);
		dac.recv_trx();
		dac.send_trx(trx_out[1]);
		dac.recv_trx();
	}

	void write_digital(uint32_t out)
	{
		out &= dg_out_mask;
		REG_WRITE(GPIO_OUT_W1TS_REG, dg_out_lut_s[out]);
		REG_WRITE(GPIO_OUT_W1TC_REG, dg_out_lut_r[out]);
	}

#pragma GCC push_options
#pragma GCC optimize("unroll-loops")
	uint32_t read_digital()
	{
		uint32_t in = REG_READ(GPIO_IN1_REG);
		uint32_t ret = 0;

		for (size_t b = 0; b < dg_in_num; ++b)
			ret |= !!(in & BIT(dig_in[b] - 32)) << b;

		return ret;
	}
#pragma GCC pop_options

	void test()
	{
		uint16_t relays = 1;
		do
		{
			uint8_t lower = relays;
			uint8_t upper = relays >> 8;

			expander_a.set_pins(0x00);
			expander_b.set_pins(0x00);

			vTaskDelay(pdMS_TO_TICKS(5));

			expander_a.set_pins(lower);
			expander_b.set_pins(upper);

			vTaskDelay(pdMS_TO_TICKS(1000));

			relays <<= 1;
		} //
		while (relays);

		expander_a.set_pins(0x00);
		expander_b.set_pins(0x00);
	}

	//

	inline uint8_t range_to_expander_steering(AnIn_Range r)
	{
		static constexpr uint8_t steering[] = {0b0000, 0b0001, 0b0010, 0b0100};
		return steering[static_cast<uint8_t>(r)];
	}

	/*/
	// you are basically a bug
	float input_multiplier(Input in)
	{
		constexpr float volt_mul[4] = {0, 1, 10, 100};	  // min range => min attn
		constexpr float curr_mul[4] = {5, 1000, 100, 10}; // min range => max gain

		constexpr float volt_ratio = 1.0f / (std::numeric_limits<int16_t>::max() >> 4 << 4) * v_ref;
		constexpr float curr_ratio = 1.0f / (std::numeric_limits<int16_t>::max() >> 4 << 4) * v_ref * 1000;

		switch (in)
		{
		case Input::In1:
			return volt_ratio * volt_mul[static_cast<uint8_t>(range_in[0])];
		case Input::In2:
			return volt_ratio * volt_mul[static_cast<uint8_t>(range_in[1])];
		case Input::In3:
			return volt_ratio * volt_mul[static_cast<uint8_t>(range_in[2])];
		case Input::In4:
			return curr_ratio / curr_mul[static_cast<uint8_t>(range_in[3])];
		default:
			return 0;
		}
	}
	//*/

	inline int16_t conv_meas(MCP3204::out_t in)
	{
		int16_t out = in;
		out <<= 16 - MCP3204::bits;
		out -= std::numeric_limits<int16_t>::min();
		return out;
	}
	inline MCP4922::in_t conv_gen(int16_t in)
	{
		uint16_t out = in;
		out += (uint16_t)std::numeric_limits<int16_t>::min();
		out >>= 16 - MCP4922::bits;
		return out;
	}

	//

	inline float measured_to_volt(int16_t val)
	{
		constexpr float ratio = 1.0f / std::numeric_limits<int16_t>::max() * v_ref;
		return val * ratio; // * attn
	}
	inline int16_t volt_to_generated(float val)
	{
		if (val >= 10.24f) [[unlikely]]
			return std::numeric_limits<int16_t>::max();
		if (val <= -10.24f) [[unlikely]]
			return std::numeric_limits<int16_t>::min();

		constexpr float ratio = std::numeric_limits<int16_t>::max() / v_ref * 4.0f / 10.0f; // / 10.24f === / v_ref * 4 / 10
		return std::round(val * ratio);
	}

	//
}