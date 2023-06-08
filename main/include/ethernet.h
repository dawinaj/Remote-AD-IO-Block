#pragma once
#include "settings.h"

#include <driver/gpio.h>

//

#define ENC28J60_SPI_HOST /* */ SPI2_HOST		  // HSPI(2) or VSPI(3)
#define ENC28J60_SPI_HZ /*   */ (5 * 1000 * 1000) // SPI_MASTER_FREQ_20M
#define ENC28J60_MOSI_GPIO /**/ GPIO_NUM_13
#define ENC28J60_MISO_GPIO /**/ GPIO_NUM_12
#define ENC28J60_SCLK_GPIO /**/ GPIO_NUM_14
#define ENC28J60_CS_GPIO /*  */ GPIO_NUM_15
#define ENC28J60_INT_GPIO /* */ GPIO_NUM_4

//

esp_err_t ethernet_init();
