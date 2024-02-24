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

class Board
{
	static constexpr const char *const TAG = "IOBoard";
	// static constexpr size_t BUF_CNT = 2;
	// static constexpr size_t BUF_LEN = 1024 * 4;

public:
	using WriteCb = std::function<bool(int16_t)>;

public:
	static constexpr float v_ref = 4.096f;

public:
	Board();
	~Board();

	esp_err_t set_input_ranges(In_Range, In_Range, In_Range, In_Range);

	esp_err_t set_input_range(Input, In_Range);

	esp_err_t execute(WriteCb &&);

	void move_config(Executing::Program &, std::vector<Generator> &);

	int16_t read_digital() const;
	void write_digital(bool, bool, bool, bool) const;

private:
	void reset_outputs();

private:
	void test();

public:
	float input_multiplier(Input) const;

public:
	static inline int16_t conv_meas(MCP3204::out_t);
	static inline MCP4922::in_t conv_gen(int16_t);

public:
	static inline float measured_to_volt(int16_t);
	static inline int16_t volt_to_generated(float);

	//
private:
	std::vector<Generator> generators;
	Executing::Program program;

	MCP23008 expander_a;
	MCP23008 expander_b;

	MCP3204 adc;
	MCP4922 dac;

	In_Range range_in[4];
	spi_transaction_t trx_in[4];
	spi_transaction_t trx_out[2];

	const gpio_num_t dig_in[4] = {GPIO_NUM_34, GPIO_NUM_35, GPIO_NUM_36, GPIO_NUM_39};
	const gpio_num_t dig_out[4] = {GPIO_NUM_4, GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_27};
};
