#include "st7789.h"
#include "t_watch_s3.h"
#include <esp_log.h>
#include <esp_err.h>
#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <driver/touch_pad.h>
#include <esp_lcd_types.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>

static const char *TAG = "st7789";

static esp_lcd_panel_handle_t panel_handle = NULL;

void st7789_init()
{
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BOARD_TFT_BL
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = BOARD_TFT_SCLK,
        .mosi_io_num = BOARD_TFT_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_TFT_WIDTH * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BOARD_TFT_DC,
        .cs_gpio_num = BOARD_TFT_CS,
        .pclk_hz = 20 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    ESP_LOGI(TAG, "Install ST7789 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_LOGI(TAG, "ST7789 panel reset");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));

    ESP_LOGI(TAG, "ST7789 panel init");
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_LOGI(TAG, "ST7789 panel on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    //ESP_LOGI(TAG, "Turn on LCD backlight");
    //gpio_set_level(BOARD_TFT_BL, 1);
}

void st7789_fillBlack()
{
    uint16_t black = 0xFFFFU;

    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 240, 240, &black);
}