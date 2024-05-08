#pragma once

#include "app_main.h"

void i2c_controller_init(peripheral_handles *peripherals);
esp_err_t i2c_writeRegister(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t value);
esp_err_t i2c_readRegister(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t *value);
uint8_t i2c_getRegister8(i2c_master_dev_handle_t dev_handle, uint8_t reg);