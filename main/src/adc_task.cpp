#include "adc_task.h"

#include "MCP3x0x.h"

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
	MCP3204 adc(SPI3_HOST, GPIO_NUM_5, 2'000'000);

	// adc.acquire_spi();

	while (1)
	{

		// int16_t voltage = adc.get_signed_raw(0, MCP_READ_SINGLE, 1);
		// ESP_LOGI("MCP3XXX", "Voltage: %hd V", voltage);
		float voltage = adc.get_float_volt(1, MCP_READ_SINGLE, 10);
		ESP_LOGI("MCP3XXX", "Voltage: %f V", voltage);

		// vTaskDelay(pdMS_TO_TICKS(100));
	}

	adc.release_spi();
}
