
idf_component_register(SRCS "main.cpp" "src/wifi.cpp" "src/webserver.cpp" "src/json_helper.cpp" "src/adc_task.cpp" # "src/ethernet.cpp"
	INCLUDE_DIRS "." "include/" # "../components/eth_enc28j60"
	REQUIRES driver lwip esp_hw_support esp_event esp_netif esp_eth esp_wifi esp_phy nvs_flash esp_http_server)

component_compile_options(-std=gnu++2a)

# "src/connection_hdlr.cpp" "src/exchange_hdlr.cpp" "src/socket_helper.cpp"