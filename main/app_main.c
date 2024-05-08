#include "t_watch_s3.h"
#include "i2c_controller.h"
#include "st7789.h"
#include "ft5436.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include <esp_log.h>

static const char *TAG = "app_main";

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
        
        for (;;)
        {
            ft5436_xy_touch(&point, &touch_cnt);
            ESP_LOGI(TAG, "X: %d, Y: %d", point.x, point.y);
            if (touch_cnt == 0U) break;
            vTaskDelay(1000U / portTICK_PERIOD_MS);
        }
        ESP_ERROR_CHECK(gpio_intr_enable(BOARD_TOUCH_INT));
        ESP_LOGI(TAG, "Reached end of touchTask");
    }
}

static void initTask(void *pvParameters)
{
    st7789_init();
    st7789_fillBlack();
    i2c_controller_init();
    ft5436_registerIsrHandler(touchIsr);

    xTaskCreatePinnedToCore(touchTask, "fTaskProcessTouch", 4096U, NULL, 2U, &touchTaskHandle, 1);

    vTaskDelete(NULL);
}

void app_main(void)
{   
    xTaskCreatePinnedToCore(initTask, "fInitTask", 4096U, NULL, 1U, NULL, 1);
    vTaskDelete(NULL);
}
