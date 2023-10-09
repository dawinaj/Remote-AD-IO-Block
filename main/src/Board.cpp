#include "Board.h"

#include <limits>
#include <cmath>

#include <esp_log.h>
#include <esp_check.h>

#include <esp_timer.h>

#define NOOP __asm__ __volatile__("nop")

Board::Board() : expander_a(I2C_NUM_0, 0b000),
				 expander_b(I2C_NUM_0, 0b001),
				 adc(SPI3_HOST, GPIO_NUM_5, 2'000'000),
				 dac(SPI2_HOST, GPIO_NUM_15, 20'000'000)
{
	expander_a.set_pins(0x00);
	expander_b.set_pins(0x00);

	range_in[0] = In_Range::OFF;
	range_in[1] = In_Range::OFF;
	range_in[2] = In_Range::OFF;
	range_in[3] = In_Range::OFF;

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
Board::~Board()
{
	adc.release_spi();
	dac.release_spi();

	expander_a.set_pins(0x00);
	expander_b.set_pins(0x00);

	ESP_LOGI(TAG, "Destructed!");
}

//

esp_err_t Board::set_input_ranges(In_Range r1, In_Range r2, In_Range r3, In_Range r4)
{
	static constexpr uint8_t steering[] = {0b0000, 0b0001, 0b0010, 0b0100};

	esp_err_t ret = ESP_OK;

	if (r1 != In_Range::Keep)
		range_in[0] = r1;
	if (r2 != In_Range::Keep)
		range_in[1] = r2;
	if (r3 != In_Range::Keep)
		range_in[2] = r3;
	if (r4 != In_Range::Keep)
		range_in[3] = r4;

	uint8_t lower = steering[static_cast<uint8_t>(range_in[0])] | steering[static_cast<uint8_t>(range_in[1])] << 4;
	uint8_t upper = steering[static_cast<uint8_t>(range_in[2])] | steering[static_cast<uint8_t>(range_in[3])] << 4;

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

esp_err_t Board::set_input_range(Input in, In_Range range)
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

const Board::ExecCfg &Board::validate_configs(GeneralCfg &&g, TriggerCfg &&t, InputCfg &&i, OutputCfg &&o)
{
	general = std::move(g);
	trigger = std::move(t);
	input = std::move(i);
	output = std::move(o);

	ExecCfg e = {
		.do_trg = true,
		.do_inp = true,
		.do_out = true,
	};

	if (trigger.port == Input::None)
		e.do_trg = false;
	if (trigger.callback == nullptr)
		e.do_trg = false;

	if (input.port_order.empty())
		e.do_inp = false;
	if (input.callback == nullptr)
		e.do_inp = false;
	if (input.repeats == 0)
		e.do_inp = false;

	if (output.voltage_gen.empty() && output.current_gen.empty())
		e.do_out = false;

	exec = e;

	return exec;
}

esp_err_t Board::execute(void *ctx)
{
	spi_transaction_t *trx1 = nullptr;
	spi_transaction_t *trx2 = nullptr;
	Generator *gen1 = nullptr;
	Generator *gen2 = nullptr;

	if (exec.do_out)
	{
		if (!output.reverse_order)
		{
			trx1 = &trx_out[0];
			trx2 = &trx_out[1];
			gen1 = &output.voltage_gen;
			gen2 = &output.current_gen;
		}
		else
		{
			trx1 = &trx_out[1];
			trx2 = &trx_out[0];
			gen1 = &output.current_gen;
			gen2 = &output.voltage_gen;
		}
	}

	//

	int64_t start = esp_timer_get_time();
	int64_t now = start;
	int64_t next = now + general.period_us;

	if (exec.do_trg)
	{
		spi_transaction_t &trx = trx_in[static_cast<uint8_t>(trigger.port) - 1];
		while (1)
		{
			while ((now = esp_timer_get_time()) < next)
				NOOP;
			next = now + general.period_us;

			adc.send_trx(trx);
			adc.recv_trx();
			if (trigger.callback(conv_meas(adc.parse_trx(trx))))
				break;
		}
	}

	//

	size_t buf_pos = 0;
	size_t buf_idx = 0;

	size_t in_idx = 0;
	size_t rpt = 0;

	Input in = Input::None;
	spi_transaction_t *trxin = nullptr;

	for (size_t iter = 0; iter < general.sample_count; ++iter)
	{
		while ((now = esp_timer_get_time()) < next)
			NOOP;
		next = now + general.period_us;

		if (exec.do_inp)
		{
			in = input.port_order[in_idx];

			trxin = &trx_in[static_cast<uint8_t>(in) - 1];
			adc.send_trx(*trxin);
		}

		if (exec.do_out)
		{
			if (gen1 != nullptr)
			{
				float volt = gen1->get(now - start);
				MCP4922::in_t code = conv_gen(volt_to_generated(volt));
				dac.write_trx(*trx1, code);
				dac.send_trx(*trx1);
				dac.recv_trx();
			}
			if (gen2 != nullptr)
			{
				float volt = gen2->get(now - start);
				MCP4922::in_t code = conv_gen(volt_to_generated(volt));
				dac.write_trx(*trx2, code);
				dac.send_trx(*trx2);
				dac.recv_trx();
			}
		}

		if (exec.do_inp)
		{
			adc.recv_trx();
			buffers[buf_idx][buf_pos] = conv_meas(adc.parse_trx(*trxin));

			if (++buf_pos == BUF_LEN)
			{
				input.callback(buffers[buf_idx].data(), BUF_LEN, ctx);
				buf_pos = 0;
				if (++buf_idx == BUF_CNT)
					buf_idx = 0;
			}

			if (++rpt == input.repeats)
			{
				rpt = 0;
				if (++in_idx == input.port_order.size())
					in_idx = 0;
			}
		}
	}

	if (buf_pos)
		input.callback(buffers[buf_idx].data(), buf_pos, ctx);

	return ESP_OK;
}

//

/*/
In_Range Board::range_decode(std::string s)
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
//*/

void Board::test()
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

inline int16_t Board::conv_meas(MCP3204::out_t in)
{
	int16_t out = in;
	out <<= 16 - MCP3204::bits;
	out -= std::numeric_limits<int16_t>::min();
	return out;
}
inline MCP4922::in_t Board::conv_gen(int16_t in)
{
	uint16_t out = in;
	out += (uint16_t)std::numeric_limits<int16_t>::min();
	out >>= 16 - MCP4922::bits;
	return out;
}

//

inline float Board::measured_to_volt(int16_t val)
{
	constexpr float ratio = 1.0f / std::numeric_limits<int16_t>::max() * v_ref;
	return val * ratio; // * attn
}
inline int16_t Board::volt_to_generated(float val)
{
	if (val >= 10.24f) [[unlikely]]
		return std::numeric_limits<int16_t>::max();
	if (val <= -10.24f) [[unlikely]]
		return std::numeric_limits<int16_t>::min();

	constexpr float ratio = std::numeric_limits<int16_t>::max() / v_ref * 4.0f / 10.0f; // / 10.24f === / v_ref * 4 / 10
	return std::round(val * ratio);
}

//
