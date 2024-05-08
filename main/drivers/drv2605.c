#include "drv2605.h"
#include "i2c_controller.h"
#include <esp_log.h>

static const char *TAG = "drv2605";

i2c_master_dev_handle_t dev_handle;

void drv2605_init(i2c_master_dev_handle_t dev)
{
    dev_handle = dev;

    i2c_writeRegister(dev_handle, DRV2605_REG_MODE, 0U);
    i2c_writeRegister(dev_handle, DRV2605_REG_LIBRARY, 1U);
    i2c_writeRegister(dev_handle, DRV2605_REG_AUDIOMAX, 100U);
    i2c_writeRegister(dev_handle, DRV2605_REG_WAVESEQ1, 1U); //Strong Clock 100%
    i2c_writeRegister(dev_handle, DRV2605_REG_WAVESEQ2, 13U); //Soft Fuzz 60%
    i2c_writeRegister(dev_handle, DRV2605_REG_WAVESEQ3, 24U); //Sharp Tick 1 100%
    i2c_writeRegister(dev_handle, DRV2605_REG_WAVESEQ4, 47U); //Buzz 1 100%
    i2c_writeRegister(dev_handle, DRV2605_REG_WAVESEQ5, 52U); //Pulsing Strong 1 100%
}