#pragma once

#include <atomic>
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
	Inv = 5,
};

enum class Output : uint8_t
{
	None = 0,
	Out1 = 1,
	Out2 = 2,
	Inv = 3,
};

enum class AnIn_Range : uint8_t
{
	OFF = 0,
	Min = 1,
	Med = 2,
	Max = 3,
};

namespace Board
{
	constexpr const char *const TAG = "IOBoard";
	// static constexpr size_t BUF_CNT = 2;
	// static constexpr size_t BUF_LEN = 1024 * 4;

	using WriteCb = std::function<bool(int16_t)>;

	constexpr float u_ref = 4.096;
	constexpr float ccvs_transresistance = 1.0;
	constexpr float vccs_transconductance = 0.1; // or 0.01?

	// constexpr int64_t volt_mul = 1'000;
	// constexpr int64_t curr_mul = 1'000'000;

	esp_err_t init();
	esp_err_t deinit();

	// esp_err_t set_input_range(Input, AnIn_Range);
	// esp_err_t disable_inputs();
	// esp_err_t enable_inputs();

	void move_config(Interpreter::Program &, std::vector<Generator> &);

	// uint32_t read_digital();
	// void write_digital(uint32_t);

	// float input_multiplier(Input);

	// public?
	// int16_t conv_meas(MCP3204::out_t);
	// MCP4922::in_t conv_gen(int16_t);

	// float measured_to_volt(int16_t);
	// int16_t volt_to_generated(float);
};
