#include "st7789.h"
#include "t_watch_s3.h"
#include "ft5436.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_types.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "lvgl.h"

#define GRAPHICS_BUFFER_SIZE (BOARD_TFT_WIDTH * BOARD_TFT_HEIGHT / 10)
#define HARDWARE_MIRROR_CORRECTION (80)
#define LVGL_COORD_CORRECTION (20)

#define LVGL_TASK_STACK_SIZE   (6 * 1024)
#define LVGL_TASK_PRIORITY     (2)
#define LVGL_TIMEOUT_MS        (10000)

static const char *TAG = "st7789";
static DMA_ATTR uint16_t buf1[GRAPHICS_BUFFER_SIZE];
static DMA_ATTR uint16_t buf2[GRAPHICS_BUFFER_SIZE];
static lv_display_t *lv_disp;
static lv_indev_t *lv_touch_indev;
static const lv_coord_t column_dsc[] = {(BOARD_TFT_WIDTH / 2) - LVGL_COORD_CORRECTION, (BOARD_TFT_WIDTH / 2) - LVGL_COORD_CORRECTION, LV_GRID_TEMPLATE_LAST};
static const lv_coord_t row_dsc[] = {(BOARD_TFT_WIDTH / 2) - LVGL_COORD_CORRECTION, (BOARD_TFT_WIDTH / 2) - LVGL_COORD_CORRECTION, LV_GRID_TEMPLATE_LAST};
static esp_timer_handle_t lvgl_tick_timer;
static TaskHandle_t lvgl_task_handle;
static lv_obj_t *debug_labels[4];

static IRAM_ATTR void touch_isr(void *arg)
{
    gpio_intr_disable(BOARD_TOUCH_INT);
    BaseType_t xYieldRequired;
    xYieldRequired = xTaskResumeFromISR(lvgl_task_handle);
    portYIELD_FROM_ISR(xYieldRequired);
}

static void lvgl_port_task(void *arg)
{
    uint32_t task_delay_ms = 0;
    uint32_t inactive_time = 0;
    for(;;)
    {
        inactive_time = lv_display_get_inactive_time(lv_disp);
        if (inactive_time < LVGL_TIMEOUT_MS)
        {
            lv_label_set_text_fmt(debug_labels[0], "%ld", inactive_time);
            task_delay_ms = lv_timer_handler();
            vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
        }
        else 
        {
            ESP_ERROR_CHECK(esp_timer_stop(lvgl_tick_timer));
            ESP_ERROR_CHECK(gpio_set_level(BOARD_TFT_BL, 0));
            ESP_ERROR_CHECK(gpio_intr_enable(BOARD_TOUCH_INT));
            vTaskSuspend(lvgl_task_handle);
            lv_tick_inc(LV_DEF_REFR_PERIOD);
            task_delay_ms = lv_timer_handler();
            ESP_ERROR_CHECK(esp_timer_restart(lvgl_tick_timer, LV_DEF_REFR_PERIOD * 1000));
            vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
            ESP_ERROR_CHECK(gpio_set_level(BOARD_TFT_BL, 1));
        }
    }
}

static void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1 + HARDWARE_MIRROR_CORRECTION;
    int offsety2 = area->y2 + HARDWARE_MIRROR_CORRECTION;
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
    lv_display_flush_ready(disp);
}

static void increase_lvgl_tick(void *arg)
{
    lv_tick_inc(LV_DEF_REFR_PERIOD);
}

static void touch_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
    ft5436_point_t point;
    uint8_t touch_cnt;
    ft5436_xy_touch(&point, &touch_cnt);

    if (touch_cnt > 0) 
    {
        data->point.x = point.x;
        data->point.y = point.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } 
    else 
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void st7789_init(peripheral_handles_t *peripherals)
{
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BOARD_TFT_BL
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    spi_bus_config_t buscfg = {
        .sclk_io_num = BOARD_TFT_SCLK,
        .mosi_io_num = BOARD_TFT_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = GRAPHICS_BUFFER_SIZE * sizeof(uint16_t)
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BOARD_TFT_DC,
        .cs_gpio_num = BOARD_TFT_CS,
        .pclk_hz = 20 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &(peripherals->st7789_handle)));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(peripherals->st7789_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(peripherals->st7789_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(peripherals->st7789_handle, true, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(peripherals->st7789_handle, true));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_TFT_BL, 1));

    lv_init();
    lv_disp = lv_display_create(BOARD_TFT_WIDTH, BOARD_TFT_HEIGHT);
    lv_display_set_buffers(lv_disp, buf1, buf2, GRAPHICS_BUFFER_SIZE * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lv_disp, flush_cb);
    lv_display_set_user_data(lv_disp, peripherals->st7789_handle);

    lv_touch_indev = lv_indev_create();
    lv_indev_set_type(lv_touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(lv_touch_indev, lv_disp);
    lv_indev_set_read_cb(lv_touch_indev, touch_cb);

    esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = increase_lvgl_tick,
        .name = "lvgl_tick",
        //.skip_unhandled_events = true
    };
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LV_DEF_REFR_PERIOD * 1000));
}

void create_lvgl_ui()
{
    lv_obj_t *scr = lv_display_get_screen_active(lv_disp);
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_style_grid_column_dsc_array(cont, column_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(cont, row_dsc, 0);
    lv_obj_set_size(cont, BOARD_TFT_WIDTH, BOARD_TFT_HEIGHT);
    lv_obj_center(cont);
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);

    lv_obj_t *label;
    lv_obj_t *obj;

    uint8_t i;
    for (i = 0; i < 4; ++i)
    {
        uint8_t col = i % 2;
        uint8_t row = i / 2;

        obj = lv_button_create(cont);
        lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
        label = lv_label_create(obj);
        debug_labels[i] = label;
        lv_label_set_text_fmt(label, "%d", i);
        lv_obj_center(label);
    }

    ft5436_register_isr_handler(touch_isr);
    ESP_ERROR_CHECK(gpio_intr_disable(BOARD_TOUCH_INT));

    xTaskCreatePinnedToCore(lvgl_port_task, "lvgl", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, &lvgl_task_handle, 1);
}