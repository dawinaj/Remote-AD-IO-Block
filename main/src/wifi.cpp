
#include "wifi.h"

// #include <algorithm>

#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_check.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>

static const char *TAG = "wifi";

//*/
#define WIFI_SSID "IO_Block"
#define WIFI_PASS "Password"
static const int WIFI_CHANNEL = 1;
static const int MAX_STA_CONN = 1;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	switch (event_id)
	{
	case WIFI_EVENT_AP_STACONNECTED:
	{
		wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
		ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
				 MAC2STR(event->mac), event->aid);
		break;
	}
	case WIFI_EVENT_AP_STADISCONNECTED:
	{
		wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
		ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
				 MAC2STR(event->mac), event->aid);
		break;
	}
	default:
		break;
	}
}

esp_err_t wifi_init()
{
	// WiFi init config
	wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();

	// WiFi config
	wifi_config_t wifi_cfg = {
		.ap = {
			.ssid = WIFI_SSID,
			.password = WIFI_PASS,
			.ssid_len = strlen(WIFI_SSID),
			.channel = WIFI_CHANNEL,
			.authmode = WIFI_AUTH_WPA2_PSK,
			.max_connection = MAX_STA_CONN,
			.pmf_cfg = {
				.required = true,
			},
		},
	};

	if (strlen(WIFI_PASS) == 0)
		wifi_cfg.ap.authmode = WIFI_AUTH_OPEN;

	// ========

	// Initialize TCP/IP network interface
	ESP_RETURN_ON_ERROR(
		esp_netif_init(),
		TAG, "Error in esp_netif_init!");

	// Create default event loop that running in background
	ESP_RETURN_ON_ERROR(
		esp_event_loop_create_default(),
		TAG, "Error in esp_event_loop_create_default!");

	esp_netif_create_default_wifi_ap();

	ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

	// Register event handers
	ESP_RETURN_ON_ERROR(
		esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL),
		TAG, "Error in esp_event_handler_instance_register!");

	// start WiFi driver
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
			 WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);

	return ESP_OK;
}
//*/

/*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sys.h"

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_SSID "UPC2206285"
#define WIFI_PASS "hB7btaadcy4n"
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		if (s_retry_num < 5)
		{
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		}
		else
		{
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG, "connect to the AP fail");
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

esp_err_t wifi_init()
{
	s_wifi_event_group = xEventGroupCreate();

	// ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
														ESP_EVENT_ANY_ID,
														&event_handler,
														NULL,
														&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
														IP_EVENT_STA_GOT_IP,
														&event_handler,
														NULL,
														&instance_got_ip));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = WIFI_SSID,
			.password = WIFI_PASS,
			.threshold = {
				.authmode = WIFI_AUTH_WPA2_PSK,
			},
			.sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
			// .sae_h2e_identifier = CONFIG_ESP_WIFI_PW_ID,
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
										   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
										   pdFALSE,
										   pdFALSE,
										   portMAX_DELAY);

	if (bits & WIFI_CONNECTED_BIT)
	{
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
				 WIFI_SSID, WIFI_PASS);
	}
	else if (bits & WIFI_FAIL_BIT)
	{
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
				 WIFI_SSID, WIFI_PASS);
	}
	else
	{
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}

	return ESP_OK;
}

//*/