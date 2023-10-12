
#include <esp_http_server.h>

#define HTTP_MEM (32 * 1024)
#define BOARD_MEM (4 * 1024)

#define HTTP_PRT (configMAX_PRIORITIES / 2)
#define BOARD_PRT (configMAX_PRIORITIES - 1)

httpd_handle_t start_webserver(void);

// void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

// void connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
