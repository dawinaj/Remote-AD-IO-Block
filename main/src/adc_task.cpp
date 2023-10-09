#include "adc_task.h"

#include "MCP3XXX.h"

//

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

		// int16_t code = board->adc.get_signed_raw(0, MCP_ADC_READ_SINGLE, 1);
		// ESP_LOGI(TAG, "Code: 0x%04x ", code);
		// vTaskDelay(pdMS_TO_TICKS(500));
		// float voltage = board->adc.get_float_volt(0, MCP_ADC_READ_SINGLE, 1);
		// ESP_LOGI(TAG, "Voltage: %f V", voltage);

		vTaskDelay(pdMS_TO_TICKS(500));
	}
}
