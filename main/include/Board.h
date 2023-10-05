#pragma once

#include "settings.h"

#include <limits>
#include <cmath>

#include <esp_log.h>
#include <esp_check.h>

#include <esp_timer.h>

#include "MCP230XX.h"
#include "MCP3XXX.h"
#include "MCP4XXX.h"

#include "Generator.h"

enum class Input : uint8_t
{
	None = 0,
	In1 = 1,
	In2 = 2,
	In3 = 3,
	In4 = 4,
};

enum class Output : uint8_t
{
	None = 0,
	In1 = 1,
	In2 = 2,
};

enum class In_Range : uint8_t
{
	OFF = 0,
	Min = 1,
	Mid = 2,
	Max = 3,
	Keep = uint8_t(-1),
};

struct InputMeasCfg
{
	typedef bool (*in_trig_fcn)(int16_t);

	std::vector<Input> *port_order;
	int16_t *buffer;
	size_t sample_count;
	size_t repeat = 1;
	size_t period_us = 0;
	int64_t time_sync = 0;
	struct
	{
		Input port = Input::None;
		in_trig_fcn callback = nullptr;
	} trigger;
	struct
	{
		Generator *voltage_gen = nullptr;
		Generator *current_gen = nullptr;
	} output;
};

struct OutputGenCfg
{
	Generator *voltage_gen = nullptr;
	Generator *current_gen = nullptr;
	size_t sample_count = 0;
	size_t period_us = 0;
	int64_t time_sync = 0;
	bool reverse_order = false;
};

class Board
{
public:
	static constexpr const char *const TAG = "IOBoard";
	MCP23008 expander_a;
	MCP23008 expander_b;

	MCP3204 adc;
	MCP4922 dac;

	In_Range range_in1 = In_Range::OFF;
	In_Range range_in2 = In_Range::OFF;
	In_Range range_in3 = In_Range::OFF;
	In_Range range_in4 = In_Range::OFF;

	spi_transaction_t trx_in[4];
	spi_transaction_t trx_out[2];

public:
	static constexpr float v_ref = 4.096f;
	// static constexpr float mv_off = 2048;

public:
	Board() : expander_a(I2C_NUM_0, 0b000),
			  expander_b(I2C_NUM_0, 0b001),
			  adc(SPI3_HOST, GPIO_NUM_5, 1'000'000),
			  dac(SPI2_HOST, GPIO_NUM_15, 10'000'000)
	{
		expander_a.set_pins(0x00);
		expander_b.set_pins(0x00);

		trx_in[0] = adc.make_trx(0);
		trx_in[1] = adc.make_trx(1);
		trx_in[2] = adc.make_trx(2);
		trx_in[3] = adc.make_trx(3);

		trx_out[0] = dac.make_trx(1);
		trx_out[1] = dac.make_trx(0);

		adc.acquire_spi();
		dac.acquire_spi();

		dac.write_trx(trx_out[0], (MCP4922::max + 1) / 2);
		dac.write_trx(trx_out[1], (MCP4922::max + 1) / 2);
		dac.send_trx(trx_out[0]);
		dac.recv_trx();
		dac.send_trx(trx_out[1]);
		dac.recv_trx();

		// setup output

		ESP_LOGI(TAG, "Constructed!");
	}
	~Board()
	{
		adc.release_spi();
		dac.release_spi();

		expander_a.set_pins(0x00);
		expander_b.set_pins(0x00);

		ESP_LOGI(TAG, "Destructed!");
	}

	esp_err_t set_input_ranges(In_Range r1, In_Range r2, In_Range r3, In_Range r4)
	{
		static constexpr uint8_t steering[] = {0b0000, 0b0001, 0b0010, 0b0100};

		esp_err_t ret = ESP_OK;

		if (r1 != In_Range::Keep)
			range_in1 = r1;
		if (r2 != In_Range::Keep)
			range_in2 = r2;
		if (r3 != In_Range::Keep)
			range_in3 = r3;
		if (r4 != In_Range::Keep)
			range_in4 = r4;

		uint8_t lower = steering[static_cast<uint8_t>(range_in1)] | steering[static_cast<uint8_t>(range_in2)] << 4;
		uint8_t upper = steering[static_cast<uint8_t>(range_in3)] | steering[static_cast<uint8_t>(range_in4)] << 4;

		ESP_GOTO_ON_ERROR(
			expander_a.set_pins(0x00),
			err, TAG, "Failed to reset relay a!");

		ESP_GOTO_ON_ERROR(
			expander_b.set_pins(0x00),
			err, TAG, "Failed to reset relay b!");

		vTaskDelay(pdMS_TO_TICKS(5));

		ESP_GOTO_ON_ERROR(
			expander_a.set_pins(lower),
			err, TAG, "Failed to set relay a!");
		ESP_GOTO_ON_ERROR(
			expander_b.set_pins(upper),
			err, TAG, "Failed to set relay b!");

		return ESP_OK;

	err:
		ESP_LOGE(TAG, "Failed to set input ranges! Error: %s", esp_err_to_name(ret));
		return ret;
	}

	esp_err_t set_input_range(Input in, In_Range range)
	{
		switch (in)
		{
		case Input::In1:
			return set_input_ranges(range, In_Range::Keep, In_Range::Keep, In_Range::Keep);
		case Input::In2:
			return set_input_ranges(In_Range::Keep, range, In_Range::Keep, In_Range::Keep);
		case Input::In3:
			return set_input_ranges(In_Range::Keep, In_Range::Keep, range, In_Range::Keep);
		case Input::In4:
			return set_input_ranges(In_Range::Keep, In_Range::Keep, In_Range::Keep, range);
		default:
			return ESP_ERR_INVALID_ARG;
		}
	}

	esp_err_t measure_inputs(InputMeasCfg &cnf)
	{
		if (cnf.port_order == nullptr || cnf.port_order->empty())
			return ESP_ERR_INVALID_ARG;
		if (cnf.buffer == nullptr)
			return ESP_ERR_INVALID_ARG;
		if (cnf.sample_count == 0)
			return ESP_OK;
		if (cnf.repeat == 0)
			return ESP_ERR_INVALID_ARG;

		bool dotrig = (cnf.trigger.port != Input::None) && (cnf.trigger.callback != nullptr) && (cnf.time_sync == 0);

		int64_t next = cnf.time_sync;
		int64_t now = esp_timer_get_time();
		if (next < now)
			next = now + cnf.period_us;

		size_t i = 0;

		if (dotrig)
		{
			spi_transaction_t &trx = trx_in[static_cast<uint8_t>(cnf.trigger.port) - 1];
			while (1)
			{
				while ((now = esp_timer_get_time()) < next)
					NOOP;
				next = now + cnf.period_us;

				adc.send_trx(trx);
				adc.recv_trx();
				if (cnf.trigger.callback(conv_meas(adc.parse_trx(trx))))
					break;
			}
		}

		while (1)
		{
			for (Input in : *cnf.port_order)
			{
				for (size_t r = cnf.repeat; r > 0; --r)
				{
					while ((now = esp_timer_get_time()) < next)
						NOOP;
					next = now + cnf.period_us;

					if (in != Input::None) [[likely]]
					{
						spi_transaction_t &trx = trx_in[static_cast<uint8_t>(in) - 1];
						adc.send_trx(trx);
						adc.recv_trx();
						cnf.buffer[i] = conv_meas(adc.parse_trx(trx));
					}
					if (++i >= cnf.sample_count) [[unlikely]]
						goto done;
				}
			}
		}
	done:
		cnf.time_sync = next;
		return ESP_OK;
	}

	//

	esp_err_t generate_outputs(OutputGenCfg &cnf)
	{
		if (cnf.voltage_gen == nullptr && cnf.current_gen == nullptr)
			return ESP_ERR_INVALID_ARG;
		if (cnf.sample_count == 0)
			return ESP_OK;

		spi_transaction_t &trx1 = trx_out[cnf.reverse_order];
		spi_transaction_t &trx2 = trx_out[!cnf.reverse_order];

		Generator *gen1 = !cnf.reverse_order ? cnf.voltage_gen : cnf.current_gen;
		Generator *gen2 = !cnf.reverse_order ? cnf.current_gen : cnf.voltage_gen;

		while (1)
			for (size_t i = 0; i < cnf.sample_count; ++i)
			{
				if (gen1 != nullptr)
				{
					MCP4922::in_t code = conv_gen(volt_to_generated(gen1->get(i)));
					dac.write_trx(trx1, code);
					dac.send_trx(trx1);
					dac.recv_trx();
				}
				if (gen2 != nullptr)
				{
					MCP4922::in_t code = conv_gen(volt_to_generated(gen2->get(i)));
					dac.write_trx(trx2, code);
					dac.send_trx(trx2);
					dac.recv_trx();
				}

				vTaskDelay(pdMS_TO_TICKS(2000));
			}
	}

	In_Range range_decode(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
					   { return std::toupper(c); });
		if (s == "MIN")
			return In_Range::Min;
		if (s == "MID")
			return In_Range::Mid;
		if (s == "MAX")
			return In_Range::Max;
		return In_Range::OFF;
	}

	void test_relays()
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

private:
public:
	static inline int16_t conv_meas(MCP3204::out_t in)
	{
		int16_t out = in;
		out <<= 16 - MCP3204::bits;
		out -= std::numeric_limits<int16_t>::min();
		return out;
	}
	static inline MCP4922::in_t conv_gen(int16_t in)
	{
		uint16_t out = in;
		out += (uint16_t)std::numeric_limits<int16_t>::min();
		out >>= 16 - MCP4922::bits;
		return out;
	}

public:
	static inline float measured_to_volt(int sum, size_t cnt)
	{
		return float(sum) / cnt / std::numeric_limits<int16_t>::max() * v_ref;
	}

	static inline float measured_to_volt(int16_t val)
	{
		constexpr float ratio = 1.0f / std::numeric_limits<int16_t>::max() * v_ref;
		return val * ratio; // * attn
	}

	static inline int16_t volt_to_generated(float val)
	{
		if (val >= 10.24f) [[unlikely]]
			return std::numeric_limits<int16_t>::max();
		if (val <= -10.24f) [[unlikely]]
			return std::numeric_limits<int16_t>::min();

		constexpr float ratio = std::numeric_limits<int16_t>::max() / v_ref * 4.0f / 10.0f; // / 10.24f === / v_ref * 4 / 10
		return std::round(val * ratio);
	}

	//
};
