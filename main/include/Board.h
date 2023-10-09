#pragma once

#include <limits>
#include <cmath>

#include "MCP230XX.h"
#include "MCP3XXX.h"
#include "MCP4XXX.h"

#include "Generator.h"

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

class Board
{
	static constexpr const char *const TAG = "IOBoard";
	static constexpr size_t BUF_CNT = 4;
	static constexpr size_t BUF_LEN = 1024 * 16;

public:
	struct ExecCfg;
	struct GeneralCfg;
	struct TriggerCfg;
	struct InputCfg;
	struct OutputCfg;

public:
	static constexpr float v_ref = 4.096f;

public:
	Board();
	~Board();

	esp_err_t set_input_ranges(In_Range, In_Range, In_Range, In_Range);
	esp_err_t set_input_range(Input, In_Range);

	esp_err_t execute(void *);

	const ExecCfg &validate_configs(GeneralCfg &&, TriggerCfg &&, InputCfg &&, OutputCfg &&);

private:
	void test();

public:
	static inline int16_t conv_meas(MCP3204::out_t);
	static inline MCP4922::in_t conv_gen(int16_t);

public:
	static inline float measured_to_volt(int16_t);
	static inline int16_t volt_to_generated(float);

	//
public:
	struct ExecCfg
	{
		bool do_trg = false;
		bool do_inp = false;
		bool do_out = false;
	};

	struct GeneralCfg
	{
		uint32_t sample_count = 0;
		uint32_t period_us = 0;
	};

	struct TriggerCfg
	{
		typedef bool (*trig_fcn)(int16_t);
		Input port = Input::None;
		trig_fcn callback = nullptr;
	};

	struct InputCfg
	{
		typedef void (*ret_fcn)(const int16_t *, size_t, void *);
		std::vector<Input> port_order;
		uint32_t repeats = 1;
		ret_fcn callback = nullptr;
		void *ctx = nullptr;
	};

	struct OutputCfg
	{
		Generator voltage_gen;
		Generator current_gen;
		bool reverse_order = false;
	};

private:
	GeneralCfg general;
	TriggerCfg trigger;
	InputCfg input;
	OutputCfg output;
	ExecCfg exec;

	MCP23008 expander_a;
	MCP23008 expander_b;

	MCP3204 adc;
	MCP4922 dac;

	In_Range range_in[4];
	spi_transaction_t trx_in[4];
	spi_transaction_t trx_out[2];

	std::array<std::array<int16_t, BUF_LEN>, BUF_CNT> buffers;
};
