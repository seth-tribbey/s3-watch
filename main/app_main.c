#include "app_main.h"
#include "t_watch_s3.h"
#include "i2c_controller.h"
#include "st7789.h"
#include "ft5436.h"
#include "drv2605.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include <esp_log.h>
#include <esp_lcd_types.h>

static const char *TAG = "app_main";
static peripheral_handles peripherals;
static TaskHandle_t touchTaskHandle;

static void logTasks()
{
    char outputBuffer[512];
    vTaskList(outputBuffer);
    ESP_LOGI(TAG, "%s", outputBuffer);
}

static void IRAM_ATTR touchIsr(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(touchTaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void touchTask(void *pvParameters)
{
    ft5436_Point point;
    uint8_t touch_cnt;

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_ERROR_CHECK(gpio_intr_disable(BOARD_TOUCH_INT));
        ft5436_xy_touch(&point, &touch_cnt);
        ESP_LOGI(TAG, "X: %d, Y: %d", point.x, point.y);
        drv2605_go();
        vTaskDelay(1000U / portTICK_PERIOD_MS);
        ESP_ERROR_CHECK(gpio_intr_enable(BOARD_TOUCH_INT));
        ESP_LOGI(TAG, "Reached end of touchTask");
    }
}

static void initTask(void *pvParameters)
{
    //Init peripherals
    i2c_controller_init(&peripherals);
    st7789_init(&peripherals);
    ft5436_registerIsrHandler(touchIsr);

    //Init lvgl

    xTaskCreatePinnedToCore(touchTask, "fTaskProcessTouch", 4096U, NULL, 2U, &touchTaskHandle, 1);

    vTaskDelete(NULL);
}

void app_main(void)
{   
    xTaskCreatePinnedToCore(initTask, "fInitTask", 4096U, NULL, 1U, NULL, 1);
    vTaskDelete(NULL);
}
