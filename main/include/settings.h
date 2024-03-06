#pragma once

// SETTINGS

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)         \
	((byte) & 0x80 ? '1' : '0'),     \
		((byte) & 0x40 ? '1' : '0'), \
		((byte) & 0x20 ? '1' : '0'), \
		((byte) & 0x10 ? '1' : '0'), \
		((byte) & 0x08 ? '1' : '0'), \
		((byte) & 0x04 ? '1' : '0'), \
		((byte) & 0x02 ? '1' : '0'), \
		((byte) & 0x01 ? '1' : '0')

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
