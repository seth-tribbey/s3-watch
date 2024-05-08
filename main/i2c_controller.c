#include "i2c_controller.h"
#include "ft5436.h"
#include "drv2605.h"
#include "t_watch_s3.h"
#include "esp_log.h"
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"

static const char *TAG = "i2c_controller";

static i2c_master_dev_handle_t ft5436_dev;
static i2c_master_dev_handle_t drv2605_dev;

void i2c_controller_init()
{
    //Buses
    i2c_master_bus_handle_t bus_handle_0;
    i2c_master_bus_handle_t bus_handle_1;
    
    i2c_master_bus_config_t i2c_mst_config_0 = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = BOARD_TOUCH_SCL,
        .sda_io_num = BOARD_TOUCH_SDA,
        .glitch_ignore_cnt = 7U,
        .flags.enable_internal_pullup = true
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config_0, &bus_handle_0));
    
    i2c_master_bus_config_t i2c_mst_config_1 = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_1,
        .scl_io_num = BOARD_I2C_SCL,
        .sda_io_num = BOARD_I2C_SDA,
        .glitch_ignore_cnt = 7U,
        .flags.enable_internal_pullup = true
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config_1, &bus_handle_1));

    //Devices
    i2c_device_config_t ft5436_config = {
        .dev_addr_length = I2C_ADDR_BIT_7,
        .device_address = FT6X36_ADDR,
        .scl_speed_hz = 100000U
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle_0, &ft5436_config, &ft5436_dev));

    i2c_device_config_t drv2605_config = {
        .dev_addr_length = I2C_ADDR_BIT_7,
        .device_address = DRV2605_SLAVE_ADDRESS,
        .scl_speed_hz = 100000U
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle_1, &drv2605_config, &drv2605_dev));

    ft5436_init(ft5436_dev, FT6X36_DEFAULT_THRESHOLD);
    drv2605_init(drv2605_dev);
}

esp_err_t i2c_writeRegister(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t value)
{
    uint8_t writeBuff[2];
	writeBuff[0] = reg;
	writeBuff[1] = value;
	return i2c_master_transmit(dev_handle, writeBuff, 2, -1);
}

esp_err_t i2c_readRegister(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t *value)
{
	return i2c_master_transmit_receive(dev_handle, &reg, 1, value, 1, -1);
}

uint8_t i2c_getRegister8(i2c_master_dev_handle_t dev_handle, uint8_t reg)
{
	uint8_t value;
	i2c_readRegister(dev_handle, reg, &value);
	return value;
}