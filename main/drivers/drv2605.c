#include "drv2605.h"
#include "i2c_controller.h"
#include <esp_log.h>

static const char *TAG = "drv2605";
static i2c_master_dev_handle_t dev_handle;

void drv2605_init(i2c_master_dev_handle_t dev)
{
    dev_handle = dev;

    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, DRV2605_REG_MODE, 0U));
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, DRV2605_REG_LIBRARY, 1U));
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, DRV2605_REG_AUDIOMAX, 100U)); //100/255 strength
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, DRV2605_REG_WAVESEQ1, 1U)); //Strong Clock 100%
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, DRV2605_REG_WAVESEQ2, 13U)); //Soft Fuzz 60%
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, DRV2605_REG_WAVESEQ3, 24U)); //Sharp Tick 1 100%
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, DRV2605_REG_WAVESEQ4, 47U)); //Buzz 1 100%
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, DRV2605_REG_WAVESEQ5, 52U)); //Pulsing Strong 1 100%
}