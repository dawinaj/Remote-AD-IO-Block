#include "adc_task.h"

#include "MCP3XXX.h"

//

static const char *TAG = "ADC Task";

static TaskHandle_t adc_task_handle = nullptr;

void start_adc_task()
{
	if (!adc_task_handle)
		xTaskCreatePinnedToCore(adc_task, "adc_task", 4096, nullptr, ADC_PRT, &adc_task_handle, CPU1);
}
void stop_adc_task()
{
	if (adc_task_handle)
		vTaskDelete(adc_task_handle);
}

void adc_task(void *arg)
{
	adc_ptr->acquire_spi();

	while (1)
	{

		// int16_t voltage = adc.get_signed_raw(0, MCP_READ_SINGLE, 1);
		// ESP_LOGI("MCP3XXX", "Voltage: %hd V", voltage);
		float voltage = adc_ptr->get_float_volt(1, MCP_ADC_READ_SINGLE, 1000);
		ESP_LOGI("MCP3xxx", "Voltage: %f V", voltage);

		// vTaskDelay(pdMS_TO_TICKS(100));
	}

	adc_ptr->release_spi();
}
