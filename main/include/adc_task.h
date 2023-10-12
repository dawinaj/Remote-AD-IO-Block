#pragma once
#include "settings.h"

// ESP-based
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// STD headers

// Local libs
// #include "etl/circular_buffer.h"

// Project headers

// TaskHandle_t adc_task_handle;

// etl::circular_buffer<std::vector<uint16_t>, 8> adc_data;

void start_adc_task();
void stop_adc_task();

void adc_task(void *arg);
