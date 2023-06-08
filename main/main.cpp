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
#include "connection_hdlr.h"
#include "exchange_hdlr.h"

#include "webserver.h"

static const char *TAG = "[" __TIME__
						 "]E🅱 ernet";

//

// #if (!defined(HAS_IPV4) || !HAS_IPV4) && (!defined(HAS_IPV6) || !HAS_IPV6)
// #error Cannot compile program without any IP family enabled!
// #endif

// StaticTask_t xchg_task_buf;
// StackType_t xchg_task_stck[XCHG_MEM];

// #if HAS_IPV4
// StaticTask_t connv4_task_buf;
// StackType_t connv4_task_stck[CONN_MEM];
// #endif

// #ifdef HAS_IPV6
// StaticTask_t connv6_task_buf;
// StackType_t connv6_task_stck[CONN_MEM];
// #endif

//

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

	//	TaskHandle_t connv4_task_hdl;
	//	TaskHandle_t connv6_task_hdl;
	//	TaskHandle_t xchg_task_hdl;

	// #if HAS_IPV4
	// 	TaskHandle_t connv4_task_hdl = xTaskCreateStaticPinnedToCore(connection_hdlr_task<AF_INET>,
	// 																 "connv4_task",
	// 																 CONN_MEM,
	// 																 (void *)&conn_queue,
	// 																 CONN_PRT,
	// 																 &connv4_task_stck[0],
	// 																 &connv4_task_buf,
	// 																 CPU0);
	// #endif

	// #ifdef HAS_IPV6
	// 	TaskHandle_t connv6_task_hdl = xTaskCreateStaticPinnedToCore(connection_hdlr_task<AF_INET6>,
	// 																 "connv6_task",
	// 																 CONN_MEM,
	// 																 (void *)&conn_queue,
	// 																 CONN_PRT,
	// 																 &connv6_task_stck[0],
	// 																 &connv6_task_buf,
	// 																 CPU0);
	// #endif

	// 	TaskHandle_t xchg_task_hdl = xTaskCreateStaticPinnedToCore(exchange_hdlr_task,
	// 															   "xchg_task",
	// 															   XCHG_MEM,
	// 															   (void *)&conn_queue,
	// 															   XCHG_PRT,
	// 															   &xchg_task_stck[0],
	// 															   &xchg_task_buf,
	// 															   CPU1);

	while (true)
	{
		ESP_LOGV(TAG, "Available memory: %" PRId32 "\tMax: %" PRId32, esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
		vTaskDelay(pdMS_TO_TICKS(20000));
	}

	vTaskSuspend(NULL);
}
