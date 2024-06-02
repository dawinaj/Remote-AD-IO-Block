#pragma once
#include "COMMON.h"

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

#define BOARD_MEM (8 * 1024)
#define BOARD_PRT (configMAX_PRIORITIES - 1)

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

	constexpr double u_ref = 4.096;
	constexpr double out_ref = u_ref / 2 / 2 * 10;

	constexpr int32_t ItoU_input = 1;
	constexpr int32_t UtoI_output = 100; // 100mA per 10V => 1V = 0.01A

	// Divider settings:       Min: 1V=>1V, Med: 10V=>1V, Max: 100V=>1V
	constexpr int32_t volt_divs[4] = {0, 1, 10, 100}; // ratios of dividers | min range => min attn
	// Gains settings: R=1Ohm; Min: 1mA=>1V, Med: 10mA=>1V, Max: 100mA=>1V
	constexpr int32_t curr_gains[4] = {5, 1000, 100, 10}; // gains of instr.amp | min range => max gain

	esp_err_t init();
	esp_err_t deinit();

	esp_err_t move_config(Interpreter::Program &, std::vector<Generator> &);

	esp_err_t give_sem_emergency();

	esp_err_t test();
};
