#pragma once

#include <esp_log.h>
#include <esp_check.h>

#include "MCP230XX.h"

#include "MCP3XXX.h"
#include "MCP4XXX.h"

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
	In_Range range_ini = In_Range::OFF;

	static constexpr int mv_ref = 4096;
	static constexpr int mv_off = 2048;

public:
	Board() : expander_a(I2C_NUM_0, 0b000),
			  expander_b(I2C_NUM_0, 0b001),
			  adc(SPI3_HOST, GPIO_NUM_5, 1'000'000),
			  dac(SPI2_HOST, GPIO_NUM_15, 10'000'000)
	{
		ESP_LOGI(TAG, "Constructed!");
	}
	~Board()
	{
		ESP_LOGI(TAG, "Destructed!");
	}

	esp_err_t set_input_ranges(In_Range r1, In_Range r2, In_Range r3, In_Range ri)
	{
		static constexpr uint8_t bits[] = {0b0000, 0b0001, 0b0010, 0b0100};

		esp_err_t ret = ESP_OK;

		if (r1 != In_Range::Keep)
			range_in1 = r1;
		if (r2 != In_Range::Keep)
			range_in2 = r2;
		if (r3 != In_Range::Keep)
			range_in3 = r3;
		if (ri != In_Range::Keep)
			range_ini = ri;

		uint8_t lower = bits[static_cast<uint8_t>(range_in1)] | bits[static_cast<uint8_t>(range_in2)] << 4;
		uint8_t upper = bits[static_cast<uint8_t>(range_in3)] | bits[static_cast<uint8_t>(range_ini)] << 4;

		ESP_GOTO_ON_ERROR(
			expander_a.set_pins(0x00),
			err, TAG, "Failed to reset relays!");

		ESP_GOTO_ON_ERROR(
			expander_b.set_pins(0x00),
			err, TAG, "Failed to reset relays!");

		vTaskDelay(pdMS_TO_TICKS(5));

		ESP_GOTO_ON_ERROR(
			expander_a.set_pins(lower),
			err, TAG, "Failed to set relays!");
		ESP_GOTO_ON_ERROR(
			expander_b.set_pins(upper),
			err, TAG, "Failed to set relays!");

		return ESP_OK;

	err:
		ESP_LOGE(TAG, "Failed to set input ranges! Error: %s", esp_err_to_name(ret));
		return ret;
	}

	esp_err_t set_input_range(uint8_t input, In_Range range)
	{
		switch (input)
		{
		case 1:
			return set_input_ranges(range, In_Range::Keep, In_Range::Keep, In_Range::Keep);
		case 2:
			return set_input_ranges(In_Range::Keep, range, In_Range::Keep, In_Range::Keep);
		case 3:
			return set_input_ranges(In_Range::Keep, In_Range::Keep, range, In_Range::Keep);
		case 4:
			return set_input_ranges(In_Range::Keep, In_Range::Keep, In_Range::Keep, range);
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
};
