#pragma once

#include "app_main.h"
#include "freertos/FreeRTOS.h"

void st7789_init(peripheral_handles_t *peripherals);
void create_lvgl_ui();