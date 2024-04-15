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

#define BOARD_MEM (4 * 1024)
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

	constexpr float u_ref = 4.096;
	constexpr float ccvs_transresistance = 1.0;
	constexpr float vccs_transconductance = 0.1; // or 0.01?

	esp_err_t init();
	esp_err_t deinit();

	esp_err_t move_config(Interpreter::Program &, std::vector<Generator> &);
};
