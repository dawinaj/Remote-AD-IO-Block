
#include "ethernet.h"

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

// static const char *WIFI_SSID = "IO_Block";
// static const char *WIFI_PASS = "Passwd";
#define WIFI_SSID "IO_Block"
#define WIFI_PASS "Password"
static const int WIFI_CHANNEL = 1;
static const int MAX_STA_CONN = 1;

/** Event handler for WiFi events */
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
