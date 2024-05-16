#include "ft5436.h"
#include "i2c_controller.h"
#include "t_watch_s3.h"
#include "driver/gpio.h"
#include <esp_log.h>
#include <esp_timer.h>

static const char *TAG = "ft5436";

static void(*ft5436_isrHandler)(void *arg) = NULL;

static i2c_master_dev_handle_t dev_handle;
static uint8_t touch_cnt;
static uint16_t touch_x[2];
static uint16_t touch_y[2];
static uint16_t touch_event[2];
static uint8_t rotation_index = 0;
static uint16_t touch_width = 0;
static uint16_t touch_height = 0;

static bool read_data(void);
static void swap(uint16_t *a, uint16_t *b);

void ft5436_init(i2c_master_dev_handle_t dev, uint8_t threshold)
{
	dev_handle = dev;

    uint8_t data_panel_id;
	ESP_ERROR_CHECK(i2c_read_register(dev_handle, FT6X36_REG_PANEL_ID, &data_panel_id));
	if (data_panel_id != FT6X36_VENDID) 
	{
		ESP_LOGE(TAG,"FT6X36_VENDID does not match. Received:0x%x Expected:0x%x\n",data_panel_id,FT6X36_VENDID);
		return;
	}

	uint8_t chip_id;
	ESP_ERROR_CHECK(i2c_read_register(dev_handle, FT6X36_REG_CHIPID, &chip_id));
	if (chip_id != FT6206_CHIPID && chip_id != FT6236_CHIPID && chip_id != FT6336_CHIPID) 
	{
		ESP_LOGE(TAG,"FT6206_CHIPID does not match. Received:0x%x",chip_id);
		return;
	}

    gpio_config_t io_conf = 
	{
        .intr_type = GPIO_INTR_NEGEDGE,
        .pin_bit_mask = 1ULL << BOARD_TOUCH_INT,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = (gpio_pulldown_t)0,
        .pull_up_en = (gpio_pullup_t)1,
    }; 
    ESP_ERROR_CHECK(gpio_config(&io_conf));
	
    esp_err_t isr_service = gpio_install_isr_service(0);
    ESP_LOGI(TAG, "ISR trigger install response: 0x%x %s", isr_service, (isr_service==0)?"ESP_OK":"");

	ESP_ERROR_CHECK(i2c_write_register(dev_handle, FT6X36_REG_DEVICE_MODE, 0x00));
	ESP_ERROR_CHECK(i2c_write_register(dev_handle, FT6X36_REG_THRESHHOLD, threshold));
	ESP_ERROR_CHECK(i2c_write_register(dev_handle, FT6X36_REG_TOUCHRATE_ACTIVE, 0x0E));
}

void ft5436_register_isr_handler(void (*fn)(void *arg)){
	ESP_ERROR_CHECK(gpio_isr_handler_add(BOARD_TOUCH_INT, fn, NULL));
	ft5436_isrHandler = fn;
}

void ft5436_xy_touch(ft5436_point_t *point, uint8_t *count)
{
	read_data();

	if (point != NULL)
	{
		point->x = touch_x[0];
		point->y = touch_y[0];
	}

	*count = touch_cnt; 
}

void ft5436_xy_event(ft5436_point_t *point, ft5436_event_t *e)
{
	read_data();
	ft5436_raw_event_t event = (ft5436_raw_event_t)touch_event[0];

	if (point != NULL)
	{
		point->x = touch_x[0];
		point->y = touch_y[0];
	}
	if (e != NULL)
	{
		switch (event)
		{
			case PressDown:
				*e = TouchStart;
				break;
			case Contact:
				*e = TouchMove;
				break;
			case LiftUp:
			default:
				*e = TouchEnd;
				break;
		}
	}
}

void ft5436_set_rotation(uint8_t rotation) 
{
	rotation_index = rotation;
}

void ft5436_set_touch_width(uint16_t width) 
{
	touch_width = width;
}

void ft5436_set_touch_height(uint16_t height) 
{
	touch_height = height;
}

static bool read_data(void)
{
    uint8_t data_size = 16U;     // Discarding last 2: 0x0E & 0x0F as not relevant
    uint8_t data[data_size];
    i2c_read_register(dev_handle, FT6X36_REG_NUM_TOUCHES, &touch_cnt);
	i2c_master_receive(dev_handle, data, data_size, -1);
	
	/*
    ESP_LOGD(TAG, "Registers:");
    for (int16_t i = 0; i < data_size; i++)
    {
        ESP_LOGD(TAG, "%x:%hx", i, data[i]);
    }
	*/

    const uint8_t addrShift = 6;
	for (uint8_t i = 0; i < 2; i++)
	{
		touch_event[i] = data[0 + i * addrShift] >> 6;
		touch_x[i] = data[1 + i * addrShift];
		touch_y[i] = data[3 + i * addrShift];
	}
	
	switch (rotation_index)
  	{
		case 1:
			swap(&touch_x[0], &touch_y[0]);
			swap(&touch_x[1], &touch_y[1]);
			touch_y[0] = touch_width - touch_y[0] -1;
			touch_y[1] = touch_width - touch_y[1] -1;
			break;
		case 2:
			touch_x[0] = touch_width - touch_x[0] - 1;
			touch_x[1] = touch_width - touch_x[1] - 1;
			touch_y[0] = touch_height - touch_y[0] - 1;
			touch_y[1] = touch_height - touch_y[1] - 1;
			break;
		case 3:
			swap(&touch_x[0], &touch_y[0]);
			swap(&touch_x[1], &touch_y[1]);
			touch_x[0] = touch_height - touch_x[0] - 1;
			touch_x[1] = touch_height - touch_x[1] - 1;
			break;
  	}
	return true;
}

static void swap(uint16_t *a, uint16_t *b) 
{
    uint16_t *t = a;
    *a = *b;
    *b = *t;
}