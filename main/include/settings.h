#pragma once

// SETTINGS

#define NDEBUG 1

#define HTTP_MEM (32 * 1024)
// #define SPI_MEM (16 * 1024) // 4096 default

#define HTTP_PRT (configMAX_PRIORITIES * 3 / 4)
#define ADC_PRT (configMAX_PRIORITIES - 1)

//

// CONSTANTS

#define CPU0 (0)
#define CPU1 (1)

#define BOUNDARY "67-B0uND4rY-67"
//

// PERMANENT INCLUDES

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <esp_log.h>

//

// TYPEDEFS

#define NOOP __asm__ __volatile__("nop")

#define LOG_MEMORY_MAX ESP_LOGD(TAG, "Max memory: %i", uxTaskGetStackHighWaterMark(NULL))

#include "Board.h"

extern Board *board;