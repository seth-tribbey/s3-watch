#pragma once

#include "app_main.h"

void i2c_controller_init(peripheral_handles_t *peripherals);
esp_err_t i2c_write_register(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t value);
esp_err_t i2c_read_register(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t *value);
uint8_t i2c_get_register8(i2c_master_dev_handle_t dev_handle, uint8_t reg);