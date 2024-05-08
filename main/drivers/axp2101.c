//Interrupt enable registers: 40,41,42
//Interrupt status registers: 48,49,4A

#include "axp2101.h"
#include "i2c_controller.h"
#include <esp_log.h>

static const char *TAG = "axp2101";
static i2c_master_dev_handle_t dev_handle;

void axp2101_init(i2c_master_dev_handle_t dev)
{
    dev_handle = dev;

    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_INPUT_CUR_LIMIT_CTRL, 0U)); //100mA current limit
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_VOFF_SET, 0U)); //2.6V power-off threshold
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_LDO_VOL0_CTRL, 0x1CU)); //RTC output voltage 3.3v
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_LDO_VOL1_CTRL, 0x1CU)); //TFT backlight output voltage 3.3v
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_LDO_VOL2_CTRL, 0x1CU)); //Touch output voltage 3.3v
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_LDO_VOL3_CTRL, 0x1CU)); //Radio output voltage 3.3v
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_LDO_VOL5_CTRL, 0x1CU)); //Vibrate output voltage 3.3v
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 1U)); //Only enable DC1
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_LDO_ONOFF_CTRL1, 0U)); //Disable DLDO2
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 0x2FU)); //Enable peripheral LDOs
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL, 0x10U)); //Set fastest on/off times
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 0xDU)); //Set correct measurement channels
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_IPRECHG_SET, 2U)); //50mA precharge current limit
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_ICC_CHG_SET, 4U)); //100mA constant current charge current limit
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_ITERM_CHG_SET_CTRL, 1U)); //25mA termination current limit
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_CV_CHG_VOL_SET, 4U)); //4.35V charge voltage limit
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_BTN_BAT_CHG_VOL_SET, 7U)); //3.3V button battery charge termination voltage
    ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 0xEU)); //Button battery charge enable
}