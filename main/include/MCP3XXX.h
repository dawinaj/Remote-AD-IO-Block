#ifndef MCP3XXX_H
#define MCP3XXX_H

#include <type_traits>

#include <esp_log.h>
#include <esp_check.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>

enum mcp_adc_channels_t : uint8_t
{
	MCP_ADC_CHANNELS_2 = 2,
	MCP_ADC_CHANNELS_4 = 4,
	MCP_ADC_CHANNELS_8 = 8,
};

enum mcp_adc_bits_t : uint8_t
{
	MCP_ADC_BITS_10 = 10,
	MCP_ADC_BITS_12 = 12,
	MCP_ADC_BITS_13 = 13,
};

enum mcp_adc_read_mode_t : bool
{
	MCP_ADC_READ_DFRNTL = 0,
	MCP_ADC_READ_SINGLE = 1
};

enum mcp_adc_signed_t : bool
{
	MCP_ADC_DATA_UNSIGNED = 0,
	MCP_ADC_DATA_SIGNED = 1
};

template <mcp_adc_channels_t C, mcp_adc_bits_t B, mcp_adc_signed_t S = MCP_ADC_DATA_UNSIGNED>
class MCP3xxx
{
	static constexpr char *TAG = "MCP3xxx";

	using uout_t = uint16_t;
	using sout_t = int16_t;

public:
	using out_t = std::conditional_t<S, sout_t, uout_t>;

private:
	spi_device_handle_t spi_hdl;
	float ref_volt;

	static constexpr uout_t resp_mask = (1u << B) - 1;
	static constexpr uout_t sign_mask = S ? (1u << (B - 1)) : 0; // only used for signed anyway, but w/e

public:
	static constexpr out_t max = S ? resp_mask / 2 : resp_mask;
	static constexpr out_t min = S ? -1 - max : 0;

	MCP3xxx(spi_host_device_t spihost, gpio_num_t csgpio, int clkhz = 1'000'000, float rv = 5.0f) : ref_volt(rv)
	{
		esp_err_t ret = ESP_OK;

		const spi_device_interface_config_t dev_cfg = {
			.command_bits = 2,
			.address_bits = (C == MCP_ADC_CHANNELS_2) ? 1 : 3,
			.dummy_bits = 2,
			.mode = 0,
			.duty_cycle_pos = 0,
			.cs_ena_pretrans = 0,
			.cs_ena_posttrans = 0,
			.clock_speed_hz = clkhz,
			.input_delay_ns = 0,
			.spics_io_num = csgpio,
			.flags = SPI_DEVICE_HALFDUPLEX,
			.queue_size = 1,
			.pre_cb = NULL,
			.post_cb = NULL,

		};

		ESP_GOTO_ON_ERROR(
			spi_bus_add_device(spihost, &dev_cfg, &spi_hdl),
			err, TAG, "Failed to add device to SPI bus!");

		// ESP_LOGW(TAG, "kHz: %i", khz);
		return;

	err:
		ESP_LOGE(TAG, "Failed to construct MCP! Error: %s", esp_err_to_name(ret));
	}
	~MCP3xxx()
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

		// spi_acq = true;

		return ESP_OK;
	}

	esp_err_t release_spi()
	{
		spi_device_release_bus(spi_hdl); // return void
		// spi_acq = false;
		return ESP_OK;
	}
	/*/
		out_t get_signed_raw(uint8_t chnl, mcp_read_mode_t rdmd = MCP_READ_SINGLE, size_t scnt = 1)
		{

			spi_transaction_t spi_trx = make_transaction(chnl, rdmd);

			int32_t sum = 0;
			int32_t raw;

			for (size_t i = 0; i < scnt; ++i)
			{
				raw = get_response(&spi_trx);
				if (raw == -1)
					goto errgs;
				sum += raw;
			}

			return (sum + ((sum < 0) ? -scnt : scnt) / 2) / scnt;

		errgs:
			return 0;
		}
	//*/

	float get_float_volt(uint8_t chnl, mcp_adc_read_mode_t rdmd = MCP_ADC_READ_SINGLE, size_t scnt = 1)
	{
		spi_transaction_t trx1 = make_transaction(chnl, rdmd);
		// spi_transaction_t trx2 = trx1;

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

private:
	inline esp_err_t send_trx(spi_transaction_t &trx)
	{
		esp_err_t ret = ESP_OK;

		ESP_GOTO_ON_ERROR(
			spi_device_polling_start(spi_hdl, &trx, portMAX_DELAY),
			err, TAG, "Error in spi_device_polling_start()");

		return ret;
	err:
		ESP_LOGE(TAG, "Error in send_trx(): %s", esp_err_to_name(ret));
		return ret;
	}

	inline esp_err_t recv_trx()
	{
		esp_err_t ret = ESP_OK;

		ESP_GOTO_ON_ERROR(
			spi_device_polling_end(spi_hdl, portMAX_DELAY),
			err, TAG, "Error in spi_device_polling_end()");

		return ret;
	err:
		ESP_LOGE(TAG, "Error in recv_trx(): %s", esp_err_to_name(ret));
		return ret;
	}

	inline out_t parse_trx(spi_transaction_t &trx)
	{
		uout_t out = SPI_SWAP_DATA_RX(*reinterpret_cast<uint32_t *>(trx.rx_data), B);

		if constexpr (S)		   // if signed
			if (out & sign_mask)   // if negative
				out |= ~resp_mask; // flip high sign bits

		return out;
	}

	spi_transaction_t make_transaction(uint8_t chnl, mcp_adc_read_mode_t rdmd)
	{
		// Request/Response format (tx/rx).
		//
		//   1 SG D2 D1 D0  0  0
		// |--|--|--|--|--|--|--|
		//
		// |--|--|--|--|--|--|--|--|--|--|--|--|
		//  12 11 10  9  8  7  6  5  4  3  2  1
		//
		// 3002      https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/21294E.pdf
		// 3004/3008 https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP3004-MCP3008-Data-Sheet-DS20001295.pdf
		// 3202      https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/21034F.pdf
		// 3204/3208 https://ww1.microchip.com/downloads/en/devicedoc/21298e.pdf
		// 3302/3304 https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/21697F.pdf

		spi_transaction_t trx;

		trx.cmd = 0b10 | rdmd; // start bit and read mode
		trx.addr = chnl;	   // &= (C == MCP_CHANNELS_2) ? 0b1 : 0b111; but wil be truncated anyway
		trx.flags = SPI_TRANS_USE_RXDATA;

		trx.tx_buffer = nullptr;
		trx.length = 0;
		trx.rxlength = B;

		return trx;
	}
};

// http://ww1.microchip.com/downloads/en/DeviceDoc/21294E.pdf
using MCP3002 = MCP3xxx<MCP_ADC_CHANNELS_2, MCP_ADC_BITS_10>;

// http://ww1.microchip.com/downloads/en/DeviceDoc/21295C.pdf
using MCP3004 = MCP3xxx<MCP_ADC_CHANNELS_4, MCP_ADC_BITS_10>;
using MCP3008 = MCP3xxx<MCP_ADC_CHANNELS_8, MCP_ADC_BITS_10>;

// http://ww1.microchip.com/downloads/en/DeviceDoc/21034D.pdf
using MCP3202 = MCP3xxx<MCP_ADC_CHANNELS_2, MCP_ADC_BITS_12>;

// http://ww1.microchip.com/downloads/en/DeviceDoc/21298c.pdf
using MCP3204 = MCP3xxx<MCP_ADC_CHANNELS_4, MCP_ADC_BITS_12>;
using MCP3208 = MCP3xxx<MCP_ADC_CHANNELS_8, MCP_ADC_BITS_12>;

// http://ww1.microchip.com/downloads/en/DeviceDoc/21697e.pdf
using MCP3302 = MCP3xxx<MCP_ADC_CHANNELS_4, MCP_ADC_BITS_13, MCP_ADC_DATA_SIGNED>;
using MCP3304 = MCP3xxx<MCP_ADC_CHANNELS_8, MCP_ADC_BITS_13, MCP_ADC_DATA_SIGNED>;

#endif