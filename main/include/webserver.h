#pragma once
#include "COMMON.h"

#define HTTP_MEM (16 * 1024)

#define HTTP_PRT (12) // must be lower than 18 of LwIP

#include <esp_http_server.h>

esp_err_t start_webserver();
esp_err_t stop_webserver();

bool httpd_req_check_live(httpd_req_t*);