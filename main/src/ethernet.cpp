
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

// #include "esp_eth_enc28j60.h"

static const char *TAG = "ethernet";

//

// static constexpr int enc28j60_spi_cs_from_hz(int clock_speed_hz)
// {
// 	if (clock_speed_hz <= 0)
// 		clock_speed_hz = 0;
// 	if (clock_speed_hz > 20 * 1000 * 1000)
// 		clock_speed_hz = 20 * 1000 * 1000;
// 	return clock_speed_hz / 1000 * CS_HOLD_TIME_MIN_NS / 1000 / 1000 + 1;
// }

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
	phy_cfg.reset_gpio_num = GPIO_NUM_NC;

	// EMAC config
	eth_esp32_emac_config_t emac_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
	emac_cfg.smi_mdc_gpio_num = GPIO_NUM_23;
	emac_cfg.smi_mdio_gpio_num = GPIO_NUM_18;

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

// ENC28J60
/*/
esp_err_t ethernet_init()
{
	esp_err_t err = ESP_OK;

	const spi_bus_config_t bus_cfg = {
		.mosi_io_num = ENC28J60_MOSI_GPIO,
		.miso_io_num = ENC28J60_MISO_GPIO,
		.sclk_io_num = ENC28J60_SCLK_GPIO,
		.quadwp_io_num = GPIO_NUM_NC,
		.quadhd_io_num = GPIO_NUM_NC,
		.data4_io_num = GPIO_NUM_NC,
		.data5_io_num = GPIO_NUM_NC,
		.data6_io_num = GPIO_NUM_NC,
		.data7_io_num = GPIO_NUM_NC,
		.max_transfer_sz = SPI_MEM,
		.flags = 0,
		.intr_flags = 0,
	};

	constexpr int delay = enc28j60_spi_cs_from_hz(ENC28J60_SPI_HZ);

	spi_device_interface_config_t device_cfg = {
		.command_bits = 3,
		.address_bits = 5,
		.dummy_bits = 0,
		.mode = 0,
		.duty_cycle_pos = 0, // 128/256
		.cs_ena_pretrans = 0,
		.cs_ena_posttrans = (uint8_t)std::min(3 * delay, 16),
		.clock_speed_hz = ENC28J60_SPI_HZ,
		.input_delay_ns = 0,
		.spics_io_num = ENC28J60_CS_GPIO,
		.flags = 0,
		.queue_size = 100,
		.pre_cb = nullptr,
		.post_cb = nullptr,
	};

	const eth_enc28j60_config_t enc28j60_cfg = {
		.spi_host_id = ENC28J60_SPI_HOST,
		.spi_devcfg = &device_cfg,
		.int_gpio_num = ENC28J60_INT_GPIO,
	};

	const eth_mac_config_t mac_cfg = {
		.sw_reset_timeout_ms = 100,
		.rx_task_stack_size = 2048,
		.rx_task_prio = (configMAX_PRIORITIES - 1),
		.flags = 0,
	};

	const eth_phy_config_t phy_cfg = {
		.phy_addr = ESP_ETH_PHY_ADDR_AUTO, // default
		.reset_timeout_ms = 100,		   // default
		.autonego_timeout_ms = 0,		   // ENC28J60 doesn't support auto-negotiation
		.reset_gpio_num = GPIO_NUM_NC,	   // ENC28J60 doesn't have a pin to reset internal PHY
	};

	const esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();

	//========

	err = spi_bus_initialize(ENC28J60_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
	if (err != ESP_OK)
		return err;

	esp_eth_mac_t *mac = esp_eth_mac_new_enc28j60(&enc28j60_cfg, &mac_cfg);
	esp_eth_phy_t *phy = esp_eth_phy_new_enc28j60(&phy_cfg);

	const esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
	esp_eth_handle_t eth_handle;
	err = esp_eth_driver_install(&eth_config, &eth_handle);
	if (err != ESP_OK)
		return err;

	// uint8_t eth_mac[6] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
	uint8_t eth_mac[6];
	err = esp_read_mac(&eth_mac[0], ESP_MAC_ETH);
	if (err != ESP_OK)
		return err;

	mac->set_addr(mac, eth_mac);

	// ENC28J60 Errata #1 check
	if (emac_enc28j60_get_chip_info(mac) < ENC28J60_REV_B5 && ENC28J60_SPI_HZ < 8 * 1000 * 1000)
	{
		ESP_LOGE(TAG, "SPI frequency must be at least 8 MHz for chip revision less than 5");
		return ESP_FAIL;
	}

	// attach Ethernet driver to TCP/IP stack
	esp_netif_t *netif_handle = esp_netif_new(&netif_cfg);
	err = esp_netif_attach(netif_handle, esp_eth_new_netif_glue(eth_handle));
	if (err != ESP_OK)
		return err;

	// Register user defined event handers
	err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL);
	if (err != ESP_OK)
		return err;

	// ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
	err = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &got_ip_event_handler, NULL);
	if (err != ESP_OK)
		return err;

	eth_duplex_t duplex = ETH_DUPLEX_FULL;
	err = esp_eth_ioctl(eth_handle, ETH_CMD_S_DUPLEX_MODE, &duplex);
	if (err != ESP_OK)
		return err;

	// start Ethernet driver state machine
	err = esp_eth_start(eth_handle);
	if (err != ESP_OK)
		return err;

	return ESP_OK;
}
//*/
//
