
#include "mcp23008.h"

#include "MCP3XXX.h"
#include "MCP4XXX.h"

enum Vin_Att
{
	OFF = 0,
	X1 = 1,
	X10 = 2,
	X100 = 4,
};

// enum Iin_Att
// {
// 	OFF = 0,
// 	X10 = 1,
// 	X100 = 2,
// 	X1000 = 4,
// }

class Board
{
private:
	mcp23008_t expander_a;
	mcp23008_t expander_b;

	MCP3204 adc;
	MCP4922 dac;

public:
	Board() : adc(SPI3_HOST, GPIO_NUM_5, 1'000'000), dac(SPI2_HOST, GPIO_NUM_15, 10'000'000)
	{
		expander_a.port = I2C_NUM_0;
		expander_a.address = address;
		expander_b.port = I2C_NUM_0;
		expander_b.address = address;

		mcp23008_init(expander_a);
		mcp23008_init(expander_b);
	}
	~Board()
	{
	}
};
