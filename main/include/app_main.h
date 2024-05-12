#pragma once

#include "driver/i2c_master.h"
#include <esp_lcd_types.h>

typedef struct 
{
    i2c_master_dev_handle_t axp2101_handle;
    i2c_master_dev_handle_t ft5436_handle;
    i2c_master_dev_handle_t drv2605_handle;
    esp_lcd_panel_handle_t st7789_handle;
} peripheral_handles_t;