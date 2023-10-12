#pragma once

// SETTINGS



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

#define LOG_MEMORY_MAX ESP_LOGD(TAG, "Max memory: %i", uxTaskGetStackHighWaterMark(NULL))

#include "Board.h"

extern Board *board;