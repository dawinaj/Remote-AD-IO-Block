#pragma once

#include <limits>
#include <cmath>
#include <functional>

#include "MCP230XX.h"
#include "MCP3XXX.h"
#include "MCP4XXX.h"

#include "Generator.h"
#include "Interpreter.h"

//

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
	Out1 = 1,
	Out2 = 2,
};

enum class In_Range : uint8_t
{
	OFF = 0,
	Min = 1,
	Med = 2,
	Max = 3,
};

namespace Board
{
	static constexpr const char *const TAG = "IOBoard";
	// static constexpr size_t BUF_CNT = 2;
	// static constexpr size_t BUF_LEN = 1024 * 4;

	using WriteCb = std::function<bool(int16_t)>;

	static constexpr float v_ref = 4.096f;

	esp_err_t init();
	esp_err_t deinit();

	esp_err_t set_input_ranges(In_Range, In_Range, In_Range, In_Range);
	esp_err_t set_input_range(Input, In_Range);

	esp_err_t execute(WriteCb &&);

	void move_config(Executing::Program &, std::vector<Generator> &);

	uint32_t read_digital();
	void write_digital(uint32_t);

	float input_multiplier(Input);

	// public?
	int16_t conv_meas(MCP3204::out_t);
	MCP4922::in_t conv_gen(int16_t);

	static inline float measured_to_volt(int16_t);
	static inline int16_t volt_to_generated(float);

};
