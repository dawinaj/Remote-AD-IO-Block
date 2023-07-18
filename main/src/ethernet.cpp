
#include "ethernet.h"

// #include <algorithm>

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_system.h>
#include <esp_check.h>
#include <esp_netif.h>
#include <esp_eth.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>

#include <driver/gpio.h>
#include <driver/spi_master.h>

static const char *TAG = "ethernet";

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

	uint8_t mac_addr[6] = {0};

	switch (event_id)
	{
	case ETHERNET_EVENT_START:
		ESP_LOGI(TAG, "Ethernet Started");
		break;
	case ETHERNET_EVENT_STOP:
		ESP_LOGI(TAG, "Ethernet Stopped");
		break;
	case ETHERNET_EVENT_CONNECTED:
		esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
		ESP_LOGI(TAG, "Ethernet Link Up");
		ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
		break;
	case ETHERNET_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "Ethernet Link Down");
		break;
	default:
		break;
	}
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
	const esp_netif_ip_info_t *ip_info = &event->ip_info;

	ESP_LOGI(TAG, "IP Event: %" PRIi32, event_id);
	ESP_LOGI(TAG, "~~~~~~~~~~~");
	ESP_LOGI(TAG, "ETHIP:  " IPSTR, IP2STR(&ip_info->ip));
	ESP_LOGI(TAG, "ETHMSK: " IPSTR, IP2STR(&ip_info->netmask));
	ESP_LOGI(TAG, "ETHGTW: " IPSTR, IP2STR(&ip_info->gw));
	ESP_LOGI(TAG, "~~~~~~~~~~~");

	// if (event_id == IP_EVENT_ETH_LOST_IP || event_id == IP_EVENT_STA_LOST_IP)
	// 	esp_restart();
}

//

esp_err_t ethernet_init()
{
	// Netif config
	esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();

	// MAC config
	eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();

	// PHY config
	eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
	phy_cfg.autonego_timeout_ms = 10000;
	phy_cfg.reset_gpio_num = GPIO_NUM_NC;

	// EMAC config
	eth_esp32_emac_config_t emac_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
	emac_cfg.smi_mdc_gpio_num = GPIO_NUM_23;
	emac_cfg.smi_mdio_gpio_num = GPIO_NUM_18;
	emac_cfg.clock_config.rmii.clock_mode = EMAC_CLK_OUT;
	emac_cfg.clock_config.rmii.clock_gpio = EMAC_CLK_OUT_180_GPIO;

	// ========

	// Initialize TCP/IP network interface
	ESP_RETURN_ON_ERROR(
		esp_netif_init(),
		TAG, "Error in esp_netif_init!");

	// Create default event loop that running in background
	ESP_RETURN_ON_ERROR(
		esp_event_loop_create_default(),
		TAG, "Error in esp_event_loop_create_default!");

	// Create new default instance of esp-netif for Ethernet
	esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

	esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_cfg, &mac_cfg);

	esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_cfg);

	esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
	esp_eth_handle_t eth_handle = NULL;

	ESP_RETURN_ON_ERROR(
		esp_eth_driver_install(&config, &eth_handle),
		TAG, "Error in esp_eth_driver_install!");

	// attach Ethernet driver to TCP/IP stack
	ESP_RETURN_ON_ERROR(
		esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)),
		TAG, "Error in esp_netif_attach!");

	// Register user defined event handers
	ESP_RETURN_ON_ERROR(
		esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL),
		TAG, "Error in esp_event_handler_register!");
	ESP_RETURN_ON_ERROR(
		esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL),
		TAG, "Error in esp_event_handler_register!");

	// start Ethernet driver state machine
	ESP_RETURN_ON_ERROR(
		esp_eth_start(eth_handle),
		TAG, "Error in esp_eth_start!");

	return ESP_OK;
}
