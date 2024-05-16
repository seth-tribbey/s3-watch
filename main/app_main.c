#include "app_main.h"
#include "t_watch_s3.h"
#include "i2c_controller.h"
#include "st7789.h"
#include "ft5436.h"
#include "drv2605.h"
#include "graphics.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"

#define INIT_TASK_STACK_SIZE   (4 * 1024)
#define INIT_TASK_PRIORITY     (1)

static const char *TAG = "app_main";
static peripheral_handles_t peripherals;

static void log_tasks()
{
    char outputBuffer[512];
    vTaskList(outputBuffer);
    ESP_LOGI(TAG, "%s", outputBuffer);
}

static void init_task(void *pv_parameters)
{
    i2c_controller_init(&peripherals);
    st7789_init(&peripherals);
    graphics_init(&peripherals);

    vTaskDelete(NULL);
}

void app_main(void)
{   
    esp_pm_config_t pm_config = 
    {
        .max_freq_mhz = 80,
        .min_freq_mhz = 80,
        .light_sleep_enable = true
    };
    /*
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
    ESP_ERROR_CHECK(gpio_sleep_sel_dis(BOARD_TOUCH_INT));
    ESP_ERROR_CHECK(gpio_wakeup_enable(BOARD_TOUCH_INT, GPIO_INTR_LOW_LEVEL));
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(BOARD_TOUCH_INT, 0));
    */

    xTaskCreatePinnedToCore(init_task, "fInitTask", INIT_TASK_STACK_SIZE, NULL, INIT_TASK_PRIORITY, NULL, 1);
    vTaskDelete(NULL);
}
