#ifndef MCP4XXX_H
#define MCP4XXX_H

#include <algorithm>
#include <type_traits>

#include <esp_log.h>
#include <esp_check.h>

#include <driver/spi_master.h>
#include <driver/gpio.h>

enum mcp_dac_channels_t : uint8_t
{
	MCP_DAC_CHANNELS_1 = 1,
	MCP_DAC_CHANNELS_2 = 2,
};

enum mcp_dac_bits_t : uint8_t
{
	MCP_DAC_BITS_8 = 8,
	MCP_DAC_BITS_10 = 10,
	MCP_DAC_BITS_12 = 12,
};

template <mcp_dac_channels_t C, mcp_dac_bits_t B>
class MCP4xxx
{
	static constexpr const char *const TAG = "MCP4xxx";

public:
	using in_t = std::conditional_t<(B <= 8), uint8_t, uint16_t>;
	static constexpr mcp_dac_bits_t bits = B;
	static constexpr mcp_adc_channels_t channels = C;

private:
	spi_device_handle_t spi_hdl;
	float ref_volt;
	bool gain_bit;
	bool refbuf_bit;

	static constexpr in_t inp_mask = (1u << B) - 1;

public:
	static constexpr in_t max = inp_mask;
	static constexpr in_t min = 0;

	MCP4xxx(spi_host_device_t spihost, gpio_num_t csgpio, int clkhz = 10'000'000, float rv = 5.0f, bool gx2 = false, bool rfb = false) : ref_volt(rv), gain_bit(!gx2), refbuf_bit(rfb)
	{
		esp_err_t ret = ESP_OK;

		const spi_device_interface_config_t dev_cfg = {
			.command_bits = 4,
			.address_bits = 0,
			.dummy_bits = 0,
			.mode = 0,
			.duty_cycle_pos = 0,
			.cs_ena_pretrans = 0,
			.cs_ena_posttrans = 0,
			.clock_speed_hz = clkhz,
			.input_delay_ns = 0,
			.spics_io_num = csgpio,
			.flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_NO_DUMMY,
			.queue_size = 1,
			.pre_cb = NULL,
			.post_cb = NULL,
		};

		ESP_GOTO_ON_ERROR(
			spi_bus_add_device(spihost, &dev_cfg, &spi_hdl),
			err, TAG, "Failed to add device to SPI bus!");

		ESP_LOGI(TAG, "Constructed with host: %d, pin: %d", spihost, csgpio);
		return;

	err:
		ESP_LOGE(TAG, "Failed to construct MCP! Error: %s", esp_err_to_name(ret));
	}
	~MCP4xxx()
	{
		esp_err_t ret = ESP_OK;

		ESP_GOTO_ON_ERROR(
			spi_bus_remove_device(spi_hdl),
			err, TAG, "Failed to remove device from SPI bus!");
		return;

	err:
		ESP_LOGE(TAG, "Failed to destruct MCP! Error: %s", esp_err_to_name(ret));
	}

	//

	esp_err_t acquire_spi(TickType_t timeout = portMAX_DELAY)
	{
		ESP_RETURN_ON_ERROR(
			spi_device_acquire_bus(spi_hdl, timeout),
			TAG, "Failed to acquire SPI bus!");

		return ESP_OK;
	}

	esp_err_t release_spi()
	{
		spi_device_release_bus(spi_hdl); // return void
		return ESP_OK;
	}

	void set_float_volt(bool chnl, float flv)
	{
		spi_transaction_t trx = make_trx(chnl);

		in_t in = std::min(flv / ref_volt, 1.0f) * max;

		write_trx(trx, in);
		send_trx(trx);
		recv_trx();
	}

	void shutdown_channel(bool chnl)
	{
		spi_transaction_t trx = make_trx(chnl, true);
		send_trx(trx);
		recv_trx();
	}

	// private:
	inline esp_err_t send_trx(spi_transaction_t &trx)
	{
		esp_err_t ret = ESP_OK;

		ESP_GOTO_ON_ERROR(
			spi_device_polling_start(spi_hdl, &trx, portMAX_DELAY),
			err, TAG, "Error in spi_device_polling_start()");

		return ESP_OK;
	err:
		ESP_LOGE(TAG, "Error in send_trx(): %s", esp_err_to_name(ret));
		return ret;
	}

	inline esp_err_t recv_trx(TickType_t timeout = portMAX_DELAY)
	{
		esp_err_t ret = spi_device_polling_end(spi_hdl, timeout);

		if (ret == ESP_OK || ret == ESP_ERR_TIMEOUT) [[likely]]
			return ret;

		ESP_LOGE(TAG, "Error in recv_trx(): %s", esp_err_to_name(ret));
		return ret;
	}

	inline void write_trx(spi_transaction_t &trx, in_t in)
	{
		*reinterpret_cast<uint32_t *>(trx.tx_data) = SPI_SWAP_DATA_TX(in, B);
	}

	spi_transaction_t make_trx(bool chnl, bool shdn = false)
	{
		// Request format (tx)
		//
		//  AB BF GA SD
		// |--|--|--|--|
		//
		//  12 11 10  9  8  7  6  5  4  3  2  1
		// |--|--|--|--|--|--|--|--|--|--|--|--|
		//
		// 4901/4911/4921 https://ww1.microchip.com/downloads/en/devicedoc/22248a.pdf page 24
		// 4902/4912/4922 https://ww1.microchip.com/downloads/en/devicedoc/22250a.pdf page 24
		//

		spi_transaction_t trx;

		trx.cmd = ((C == MCP_DAC_CHANNELS_2) ? chnl << 3 : 0) | refbuf_bit << 2 | gain_bit << 1 | !shdn; // not shutdown
		trx.flags = SPI_TRANS_USE_TXDATA;

		trx.rx_buffer = nullptr;
		trx.length = B;
		trx.rxlength = 0;

		return trx;
	}
};

// https://ww1.microchip.com/downloads/en/DeviceDoc/22244B.pdf

using MCP4801 = MCP4xxx<MCP_DAC_CHANNELS_1, MCP_DAC_BITS_8>;
using MCP4811 = MCP4xxx<MCP_DAC_CHANNELS_1, MCP_DAC_BITS_10>;
using MCP4821 = MCP4xxx<MCP_DAC_CHANNELS_1, MCP_DAC_BITS_12>;

// https://ww1.microchip.com/downloads/en/DeviceDoc/20002249B.pdf

using MCP4802 = MCP4xxx<MCP_DAC_CHANNELS_2, MCP_DAC_BITS_8>;
using MCP4812 = MCP4xxx<MCP_DAC_CHANNELS_2, MCP_DAC_BITS_10>;
using MCP4822 = MCP4xxx<MCP_DAC_CHANNELS_2, MCP_DAC_BITS_12>;

//

// https://ww1.microchip.com/downloads/en/devicedoc/22248a.pdf

using MCP4901 = MCP4xxx<MCP_DAC_CHANNELS_1, MCP_DAC_BITS_8>;
using MCP4911 = MCP4xxx<MCP_DAC_CHANNELS_1, MCP_DAC_BITS_10>;
using MCP4921 = MCP4xxx<MCP_DAC_CHANNELS_1, MCP_DAC_BITS_12>;

// https://ww1.microchip.com/downloads/en/devicedoc/22250a.pdf

using MCP4902 = MCP4xxx<MCP_DAC_CHANNELS_2, MCP_DAC_BITS_8>;
using MCP4912 = MCP4xxx<MCP_DAC_CHANNELS_2, MCP_DAC_BITS_10>;
using MCP4922 = MCP4xxx<MCP_DAC_CHANNELS_2, MCP_DAC_BITS_12>;

#endif