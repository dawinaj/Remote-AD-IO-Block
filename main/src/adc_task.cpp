#include "adc_task.h"

static const char *TAG = "ADC Task";

static TaskHandle_t adc_task_handle = nullptr;

void start_adc_task()
{
	if (!adc_task_handle)
		xTaskCreatePinnedToCore(adc_task, "adc_task", 4096, nullptr, ADC_PRT, &adc_task_handle, CPU0);
}
void stop_adc_task()
{
	if (adc_task_handle)
		vTaskDelete(adc_task_handle);
}

void adc_task(void *arg)
{
	while (1)
	{
		ESP_LOGI(TAG, "doing some shit");
		vTaskDelay(pdMS_TO_TICKS(10000));
	}
}
