#include "graphics.h"
#include "t_watch_s3.h"
#include "axp2101.h"
#include "ft5436.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

#define HARDWARE_MIRROR_CORRECTION (80)
#define LVGL_COORD_CORRECTION (10)
#define LVGL_TASK_STACK_SIZE (6 * 1024)
#define LVGL_TASK_PRIORITY (2)
#define LVGL_TIMEOUT_MS (10000)
#define LVGL_UI_SLOWTICK_MS (1000)

static const char *TAG = "graphics";
static DMA_ATTR uint16_t buf1[GRAPHICS_BUFFER_SIZE];
static DMA_ATTR uint16_t buf2[GRAPHICS_BUFFER_SIZE];
static lv_display_t *lv_disp;
static lv_indev_t *lv_touch_indev;
static lv_obj_t *pwr_lbl;
static esp_timer_handle_t lvgl_tick_timer;
static TaskHandle_t lvgl_task_handle;

static lv_color_t red_color = 
{
    .red = 0xff,
    .green = 0x00,
    .blue = 0x00
};

static lv_color_t green_color = 
{
    .red = 0x00,
    .green = 0xff,
    .blue = 0x00
};

static lv_color_t blue_color = 
{
    .red = 0x00,
    .green = 0x00,
    .blue = 0xff
};

static lv_obj_t *debug_labels[4];
static char report_buffer[512];

static void print_stats()
{
    for(;;)
    {
        vTaskGetRunTimeStats(report_buffer);
        ESP_LOGI(TAG, "%s", report_buffer);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static IRAM_ATTR void touch_isr(void *arg)
{
    gpio_intr_disable(BOARD_TOUCH_INT);
    BaseType_t xYieldRequired;
    xYieldRequired = xTaskResumeFromISR(lvgl_task_handle);
    portYIELD_FROM_ISR(xYieldRequired);
}

static void lvgl_port_task(void *arg)
{
    uint16_t task_delay_ms = 0;
    uint16_t inactive_time = 0;
    uint16_t slow_tick_counter = 0;
    for(;;)
    {
        //inactive_time = lv_display_get_inactive_time(lv_disp);
        if (inactive_time < LVGL_TIMEOUT_MS)
        {
            task_delay_ms = lv_timer_handler();
            vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
            slow_tick_counter += task_delay_ms;

            if (slow_tick_counter >= LVGL_UI_SLOWTICK_MS)
            {
                slow_tick_counter = 0;
                lv_label_set_text_fmt(pwr_lbl, "%d%%", axp2101_get_battery_percentage());
            }
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

//check px_map for color order
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

static void button_pressed_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_set_style_bg_color(target, red_color, LV_PART_MAIN);
    lv_obj_invalidate(target);
    ESP_LOGI(TAG, "%d", code);
}

static void button_released_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_set_style_bg_color(target, green_color, LV_PART_MAIN);
    lv_obj_invalidate(target);
    ESP_LOGI(TAG, "%d", code);
}

void graphics_init(peripheral_handles_t *peripherals)
{
    //Init LVGL
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
        .name = "lvgl_tick"
    };
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LV_DEF_REFR_PERIOD * 1000));
    
    //Create UI
    lv_obj_t *scr = lv_display_get_screen_active(lv_disp);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_style_bg_color(cont, lv_color_black(), LV_PART_MAIN);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(cont, BOARD_TFT_WIDTH, BOARD_TFT_HEIGHT);
    lv_obj_center(cont);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    pwr_lbl = lv_label_create(cont);
    lv_obj_set_style_text_color(pwr_lbl, lv_color_white(), LV_PART_MAIN);

    lv_obj_t *label;
    lv_obj_t *button;
    uint8_t i;
    for (i = 0; i < 4; ++i)
    {
        uint8_t col = i % 2;
        uint8_t row = i / 2;

        button = lv_obj_create(cont);
        lv_obj_set_style_bg_color(button, blue_color, LV_PART_MAIN);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(button, button_pressed_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(button, button_released_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_set_size(button, LV_PCT(48), LV_PCT(42));
        if (i == 0) lv_obj_add_flag(button, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
        label = lv_label_create(button);
        debug_labels[i] = label;
        lv_label_set_text_fmt(label, "%d", i);
        lv_obj_center(label);
        lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
    }

    //Start LVGL loop
    ft5436_register_isr_handler(touch_isr);
    ESP_ERROR_CHECK(gpio_intr_disable(BOARD_TOUCH_INT));

    xTaskCreatePinnedToCore(lvgl_port_task, "lvgl", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, &lvgl_task_handle, 1);
    
    ESP_ERROR_CHECK(gpio_set_level(BOARD_TFT_BL, 1));

    //xTaskCreatePinnedToCore(print_stats, "print_stats", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL, 0);
}