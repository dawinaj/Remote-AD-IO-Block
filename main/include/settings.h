#pragma once

// SETTINGS

// #define HAS_IPV4 1
// #define HAS_IPV6 1

#define RAPIDJSON_HAS_STDSTRING 1
#define RAPIDJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY (1024)
#define NDEBUG 1

#define HTTP_MEM (32 * 1024)
#define SPI_MEM (16 * 1024) // 4096 default

#define INP_BUF (1024)
#define OUT_BUF (1024)

#define HTTP_PRT (configMAX_PRIORITIES * 3 / 4)
#define ETHR_PRT (configMAX_PRIORITIES - 1)

//

// CONSTANTS

#define CPU0 (0)
#define CPU1 (1)

//

// PERMANENT INCLUDES

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

//

// TYPEDEFS

#define LOG_MEMORY_MAX ESP_LOGD(TAG, "Max memory: %i", uxTaskGetStackHighWaterMark(NULL))

// #include "msd/channel.hpp"
// using conn_list_t = msd::channel<int>;
