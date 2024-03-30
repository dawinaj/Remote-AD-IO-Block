
#define HTTP_MEM (16 * 1024)
#define BOARD_MEM (4 * 1024)

#define HTTP_PRT (configMAX_PRIORITIES / 2)
#define BOARD_PRT (configMAX_PRIORITIES - 1)

#include <esp_http_server.h>

esp_err_t start_webserver();
esp_err_t stop_webserver();
