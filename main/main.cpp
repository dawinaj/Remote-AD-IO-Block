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

//

static const char *TAG = "[" __TIME__ "]E🅱 ernet";

//

void init_spi()
{
	spi_bus_config_t bus_cfg = {
		.mosi_io_num = GPIO_NUM_23,
		.miso_io_num = GPIO_NUM_19,
		.sclk_io_num = GPIO_NUM_18,
		.quadwp_io_num = GPIO_NUM_NC,
		.quadhd_io_num = GPIO_NUM_NC,
		.data4_io_num = GPIO_NUM_NC,
		.data5_io_num = GPIO_NUM_NC,
		.data6_io_num = GPIO_NUM_NC,
		.data7_io_num = GPIO_NUM_NC,
		.max_transfer_sz = 0,
		.flags = SPICOMMON_BUSFLAG_MASTER,
	};

	spi_bus_initialize(SPI3_HOST, &bus_cfg, 0);
}

extern "C" void app_main(void)
{
	ESP_LOGI(TAG, "H E N L O B E N C, Matte kudasai");

	esp_log_level_set("*", ESP_LOG_INFO);
	// esp_log_level_set("WEBSOCKET_CLIENT", ESP_LOG_DEBUG);
	// esp_log_level_set("TRANS_TCP", ESP_LOG_DEBUG);
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

	// vTaskDelay(pdMS_TO_TICKS(100));
	// ESP_ERROR_CHECK(ethernet_init());
	// ESP_LOGI(TAG, "ETHERNET  init done");

	vTaskDelay(pdMS_TO_TICKS(100));
	ESP_ERROR_CHECK(wifi_init());
	ESP_LOGI(TAG, "WIFI      init done");

	// gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);
	// gpio_set_direction(GPIO_NUM_19, GPIO_MODE_INPUT);

	init_spi();

	start_adc_task();

	vTaskDelay(pdMS_TO_TICKS(1000));

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
