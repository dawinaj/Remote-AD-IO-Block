#ifndef MCP23XXX_H
#define MCP23XXX_H

#include <esp_err.h>
#include <esp_log.h>

#include "i2c_manager.h"

//

class MCP23008
{
	static constexpr char *TAG = "MCP230xx";

	i2c_port_t port; // I2C_NUM_0 or I2C_NUM_1
	uint8_t address; // Hardware address of the device
	uint8_t bits;

private:
	enum class CMD : uint8_t
	{
		IODIR = 0x00,
		IPOL = 0x01,
		GPINTEN = 0x02,
		DEFVAL = 0x03,
		INTCON = 0x04,
		IOCON = 0x05,
		GPPU = 0x06,
		INTF = 0x07,
		INTCAP = 0x08,
		GPIO = 0x09,
		OLAT = 0x0A,
	};

public:
	MCP23008(i2c_port_t p, uint8_t a = 0b0100000) : port(p), address(a)
	{
		address &= 0b111;
		address |= 0b0100000;

		ESP_LOGI(TAG, "Constructed with port: %d, address: %d", port, address);

		write_reg(CMD::IODIR, 0x00);
	}
	~MCP23008()
	{
	}

	// helpers
private:
	esp_err_t read_reg(uint8_t reg, uint8_t &d)
	{
		return i2c_manager_read(mcp->port, mcp->address, reg, &d, 1);
	}

	esp_err_t write_reg(uint8_t reg, uint8_t d)
	{
		return i2c_manager_write(mcp->port, mcp->address, reg, &d, 1);
	}
};

#endif