#pragma once

#include <esp_log.h>
#include <esp_check.h>

#include "MCP230XX.h"

#include "MCP3XXX.h"
#include "MCP4XXX.h"

enum class Input : uint8_t
{
	None = 0,
	In1 = 1,
	In2 = 2,
	In3 = 3,
	In4 = 4,
};

enum class In_Range : uint8_t
{
	OFF = 0,
	Min = 1,
	Mid = 2,
	Max = 3,
	Keep = uint8_t(-1),
};

class Board
{
private:
	static constexpr const char *const TAG = "IOBoard";
	MCP23008 expander_a;
	MCP23008 expander_b;

	MCP3204 adc;
	MCP4922 dac;

	In_Range range_in1 = In_Range::OFF;
	In_Range range_in2 = In_Range::OFF;
	In_Range range_in3 = In_Range::OFF;
	In_Range range_in4 = In_Range::OFF;

	static constexpr float v_ref = 4.096f;
	// static constexpr float mv_off = 2048;

	spi_transaction_t trx_in1;
	spi_transaction_t trx_in2;
	spi_transaction_t trx_in3;
	spi_transaction_t trx_in4;
	spi_transaction_t trx_out1;
	spi_transaction_t trx_out2;

public:
	Board() : expander_a(I2C_NUM_0, 0b000),
			  expander_b(I2C_NUM_0, 0b001),
			  adc(SPI3_HOST, GPIO_NUM_5, 1'000'000),
			  dac(SPI2_HOST, GPIO_NUM_15, 10'000'000)
	{

		trx_in1 = adc.make_transaction(0);
		trx_in2 = adc.make_transaction(1);
		trx_in3 = adc.make_transaction(2);
		trx_in4 = adc.make_transaction(3);

		trx_out1 = dac.make_transaction(0);
		trx_out2 = dac.make_transaction(1);

		// setup output

		ESP_LOGI(TAG, "Constructed!");
	}
	~Board()
	{
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
		if (ri != In_Range::Keep)
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

	esp_err_t measure_inputs(size_t cnt, Input i1, Input i2 = Input::None, Input i3 = Input::None, Input i4 = Input::None)
	{
		bool b1 = i1 != Input::None;
		bool b2 = i2 != Input::None;
		bool b3 = i3 != Input::None;
		bool b4 = i4 != Input::None;

		if (!b1 && !b2 && !b3 && !b4)
			return ESP_ERR_INVALID_ARG;

		int32_t sum = 0;

		for (size_t i = 0; i < scnt; ++i)
		{
			send_trx(trx1);
			recv_trx();
			sum += parse_trx(trx1);
		}

		/*/
		size_t i = 0;

		send_trx(trx1);
		++i;

		while (i < scnt)
		{
			recv_trx();
			send_trx(trx2);
			++i;
			sum += parse_trx(trx1);
			recv_trx();
			if (i < scnt)
			{
				send_trx(trx1);
				++i;
			}
			sum += parse_trx(trx2);
		}
		//*/

		return sum * ref_volt / scnt / max;
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
	inline int16_t convert_measured(int sum, size_t cnt)
	{
		sum <<= 16 - MCP3204::B;
		sum /= cnt;
		sum -= 1 << 15;
		return sum;
	}
	inline float measured_to_volt(int sum, size_t cnt)
	{
		// ((vin / attn * 0.5) + (vref*0.5)) / vref * code_max = code
		// =>
		// vin / attn * 0.5 = code/code_max*vref - (vref*0.5)
		// =>
		// vin = (code/code_max*vref - (vref*0.5)) * attn * 2
		// return (code * v_ref / cnt / MCP3204::max - v_ref * 0.5) * 2; // * attn
		return (2.0f * sum / cnt / MCP3204::max - 1.0f) * v_ref; // * attn
	}

	//
};
