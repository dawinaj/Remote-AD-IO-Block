#include "settings.h"

// #include <cstdio>
// #include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <driver/gpio.h>
#include <driver/spi_master.h>

#include <nvs_flash.h>
// #include <esp_netif.h>
// #include <esp_event.h>
// #include <esp_eth.h>

// #include "ethernet.h"
#include "wifi.h"
#include "webserver.h"
#include "adc_task.h"

#include "MCP4XXX.h"

//

static const char *TAG = "[" __TIME__ "]E🅱 ernet";

//

void init_spi()
{
	spi_bus_config_t bus2_cfg = {
		.mosi_io_num = GPIO_NUM_13,
		.miso_io_num = GPIO_NUM_NC,
		.sclk_io_num = GPIO_NUM_14,
		.data2_io_num = GPIO_NUM_NC,
		.data3_io_num = GPIO_NUM_NC,
		.data4_io_num = GPIO_NUM_NC,
		.data5_io_num = GPIO_NUM_NC,
		.data6_io_num = GPIO_NUM_NC,
		.data7_io_num = GPIO_NUM_NC,
		.max_transfer_sz = 0,
		.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_IOMUX_PINS,
		.intr_flags = 0,
	};

	spi_bus_config_t bus3_cfg = {
		.mosi_io_num = GPIO_NUM_23,
		.miso_io_num = GPIO_NUM_19,
		.sclk_io_num = GPIO_NUM_18,
		.data2_io_num = GPIO_NUM_NC,
		.data3_io_num = GPIO_NUM_NC,
		.data4_io_num = GPIO_NUM_NC,
		.data5_io_num = GPIO_NUM_NC,
		.data6_io_num = GPIO_NUM_NC,
		.data7_io_num = GPIO_NUM_NC,
		.max_transfer_sz = 0,
		.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_IOMUX_PINS,
		.intr_flags = 0,
	};

	spi_bus_initialize(SPI2_HOST, &bus2_cfg, SPI_DMA_DISABLED);

	spi_bus_initialize(SPI3_HOST, &bus3_cfg, SPI_DMA_DISABLED);
}

MCP3204 *adc_ptr;
MCP4922 *dac_ptr;

extern "C" void app_main(void)
{
	ESP_LOGI(TAG, "H E N L O B E N C, Matte kudasai");

	esp_log_level_set("*", ESP_LOG_INFO);
	vTaskDelay(pdMS_TO_TICKS(100));

	ESP_ERROR_CHECK(gpio_install_isr_service(0));
	ESP_LOGI(TAG, "GPIO_ISR  init done");

	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	ESP_LOGI(TAG, "NVS_FLASH init done");

	vTaskDelay(pdMS_TO_TICKS(100));
	ESP_ERROR_CHECK(wifi_init());
	ESP_LOGI(TAG, "WIFI      init done");

	// gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);
	// gpio_set_direction(GPIO_NUM_19, GPIO_MODE_INPUT);

	init_spi();

	MCP3204 adc(SPI3_HOST, GPIO_NUM_5, 2'000'000);
	MCP4922 dac(SPI2_HOST, GPIO_NUM_15, 10'000'000);
	adc_ptr = &adc;
	dac_ptr = &dac;

	// while (1)
	// {
	dac.set_float_volt(0, 4.0f);
	dac.set_float_volt(1, 2.137f);
	vTaskDelay(pdMS_TO_TICKS(1000));
	// }

	start_adc_task();
	vTaskDelay(pdMS_TO_TICKS(4000));
	stop_adc_task();

	vTaskSuspend(NULL);

	httpd_handle_t server = start_webserver();

	while (true)
	{
		ESP_LOGV(TAG, "Available memory: %" PRId32 "\tMax: %" PRId32, esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
		vTaskDelay(pdMS_TO_TICKS(20000));
	}

	vTaskSuspend(NULL);
}
