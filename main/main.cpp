#include "settings.h"

// #include <cstdio>
// #include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <driver/gpio.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_eth.h>

#include <lwip/sockets.h>

#include "ethernet.h"

#include "webserver.h"

static const char *TAG = "[" __TIME__ "]E🅱 ernet";

extern "C" void app_main(void)
{
	ESP_LOGI(TAG, "H E N L O B E N C, Wait");

	esp_log_level_set("*", ESP_LOG_VERBOSE);
	esp_log_level_set("WEBSOCKET_CLIENT", ESP_LOG_DEBUG);
	esp_log_level_set("TRANS_TCP", ESP_LOG_DEBUG);
	vTaskDelay(pdMS_TO_TICKS(100));

	ESP_ERROR_CHECK(gpio_install_isr_service(0));
	ESP_LOGI(TAG, "GPIO_ISR  init done");

	// gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);
	// gpio_set_direction(GPIO_NUM_19, GPIO_MODE_INPUT);

	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_LOGI(TAG, "NVS_FLASH init done");

	vTaskDelay(pdMS_TO_TICKS(100));
	ESP_ERROR_CHECK(ethernet_init());
	ESP_LOGI(TAG, "ETHERNET  init done");

	// conn_list_t conn_queue;

	static httpd_handle_t server = nullptr;

	// ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
	// ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));

	server = start_webserver();

	while (true)
	{
		ESP_LOGV(TAG, "Available memory: %" PRId32 "\tMax: %" PRId32, esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
		vTaskDelay(pdMS_TO_TICKS(20000));
	}

	vTaskSuspend(NULL);
}
