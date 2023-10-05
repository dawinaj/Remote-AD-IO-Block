
idf_component_register(
	SRCS
	"main.cpp"
	"src/wifi.cpp"
	"src/webserver.cpp"
	"src/json_helper.cpp"
	"src/adc_task.cpp"
	"src/Generator.cpp"
	"src/GenSignal.cpp"
	INCLUDE_DIRS
	"."
	"include/"
	REQUIRES
	driver # i2c spi
	esp_timer # timer
	esp_event # wifi
	esp_netif # wifi
	esp_wifi # wifi
	esp_http_server # server
	i2c_manager # mcp23008
)

component_compile_options(-std=gnu++2a)
