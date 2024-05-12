#include "app_main.h"
#include "t_watch_s3.h"
#include "i2c_controller.h"
#include "st7789.h"
#include "ft5436.h"
#include "drv2605.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "lvgl.h"
#include <esp_log.h>
#include <esp_lcd_types.h>

#define INIT_TASK_STACK_SIZE   (4 * 1024)
#define INIT_TASK_PRIORITY     (1)

#define LVGL_TASK_MAX_DELAY_MS (500)
#define LVGL_TASK_MIN_DELAY_MS (1)
#define LVGL_TASK_STACK_SIZE   (6 * 1024)
#define LVGL_TASK_PRIORITY     (2)

static const char *TAG = "app_main";
static peripheral_handles_t peripherals;
static TaskHandle_t lvgl_task_handle;

static void log_tasks()
{
    char outputBuffer[512];
    vTaskList(outputBuffer);
    ESP_LOGI(TAG, "%s", outputBuffer);
}

static void lvgl_port_task(void *arg)
{
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    for(;;)
    {
        task_delay_ms = lv_timer_handler();
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) 
        {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } 
        else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) 
        {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

static void init_task(void *pv_parameters)
{
    i2c_controller_init(&peripherals);
    st7789_init(&peripherals);
    create_lvgl_ui();

    xTaskCreatePinnedToCore(lvgl_port_task, "lvgl", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, &lvgl_task_handle, 1);

    vTaskDelete(NULL);
}

void app_main(void)
{   
    xTaskCreatePinnedToCore(init_task, "fInitTask", INIT_TASK_STACK_SIZE, NULL, INIT_TASK_PRIORITY, NULL, 1);
    vTaskDelete(NULL);
}
