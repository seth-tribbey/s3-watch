#include "ft5436.h"
#include "i2c_controller.h"
#include "t_watch_s3.h"
#include "driver/gpio.h"
#include <esp_log.h>
#include <esp_timer.h>

static const char *TAG = "ft5436";

static void(*ft5436_isrHandler)(void *arg) = NULL;
static void(*ft5436_touchHandler)(ft5436_Point point, ft5436_Event e) = NULL;

static i2c_master_dev_handle_t dev_handle;
static uint8_t touch_cnt;

// Make touch rotation aware:
static uint8_t ft5436_rotation = 0;
static uint16_t ft5436_touch_width = 0;
static uint16_t ft5436_touch_height = 0;
static uint8_t ft5436_touches;
static uint16_t ft5436_touchX[2], ft5436_touchY[2], ft5436_touchEvent[2];
static ft5436_Point ft5436_points[10];
static uint8_t ft5436_pointIdx = 0;
static unsigned long ft5436_touchStartTime = 0;
static unsigned long ft5436_touchEndTime = 0;
static uint8_t ft5436_lastEvent = 3; // No event
static uint16_t ft5436_lastX = 0;
static uint16_t ft5436_lastY = 0;
static bool ft5436_dragMode = false;
static const uint8_t ft5436_maxDeviation = 5;

static uint8_t ft5436_touched();
static void ft5436_loop();
static void ft5436_processTouch();
static bool ft5436_readData(void);
static void ft5436_fireEvent(ft5436_Point point, ft5436_Event e);
static void ft5436_debugInfo();
static void ft5436_setRotation(uint8_t rotation);
static void ft5436_setTouchWidth(uint16_t width);
static void ft5436_setTouchHeight(uint16_t height);
static void ft5436_swap(uint16_t *a, uint16_t *b);

void ft5436_init(i2c_master_dev_handle_t dev, uint8_t threshold)
{
	dev_handle = dev;

    uint8_t data_panel_id;
	ESP_ERROR_CHECK(i2c_readRegister(dev_handle, FT6X36_REG_PANEL_ID, &data_panel_id));

	if (data_panel_id != FT6X36_VENDID) {
		ESP_LOGE(TAG,"FT6X36_VENDID does not match. Received:0x%x Expected:0x%x\n",data_panel_id,FT6X36_VENDID);
		return;
		}
		ESP_LOGI(TAG, "\tDevice ID: 0x%02x", data_panel_id);

	uint8_t chip_id;
	ESP_ERROR_CHECK(i2c_readRegister(dev_handle, FT6X36_REG_CHIPID, &chip_id));
	if (chip_id != FT6206_CHIPID && chip_id != FT6236_CHIPID && chip_id != FT6336_CHIPID) {
		ESP_LOGE(TAG,"FT6206_CHIPID does not match. Received:0x%x",chip_id);
		return;
	}
	ESP_LOGI(TAG, "Found touch controller with Chip ID: 0x%02x", chip_id);

    // INT pin triggers the callback function on the Falling edge of the GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE, //GPIO_INTR_NEGEDGE repeats always interrupt
        .pin_bit_mask = 1ULL << BOARD_TOUCH_INT,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = (gpio_pulldown_t)0, // disable pull-down mode
        .pull_up_en = (gpio_pullup_t)1, //pull-up mode
    }; 
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    esp_err_t isr_service = gpio_install_isr_service(0);
    ESP_LOGI(TAG, "ISR trigger install response: 0x%x %s", isr_service, (isr_service==0)?"ESP_OK":"");

	ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, FT6X36_REG_DEVICE_MODE, 0x00));
	ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, FT6X36_REG_THRESHHOLD, threshold));
	ESP_ERROR_CHECK(i2c_writeRegister(dev_handle, FT6X36_REG_TOUCHRATE_ACTIVE, 0x0E));
}

void ft5436_registerIsrHandler(void (*fn)(void *arg)){
	ESP_ERROR_CHECK(gpio_isr_handler_add(BOARD_TOUCH_INT, fn, NULL));
	ft5436_isrHandler = fn;
	ESP_LOGI(TAG, "ISR handler function registered");
}

void ft5436_registerTouchHandler(void (*fn)(ft5436_Point point, ft5436_Event e))
{
	ft5436_touchHandler = fn;
	ESP_LOGI(TAG, "Touch handler function registered");
}

static uint8_t ft5436_touched()
{
	uint8_t data_buf;
	esp_err_t ret = i2c_readRegister(dev_handle, FT6X36_REG_NUM_TOUCHES, &data_buf);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Error reading from device: %s", esp_err_to_name(ret));
	 }

	if (data_buf > 2)
	{
		data_buf = 0;
	}

	return data_buf;
}

static void ft5436_loop()
{
	ft5436_processTouch();
}

static void ft5436_processTouch()
{
	ft5436_readData();
	uint8_t n = 0;
	ft5436_RawEvent event = (ft5436_RawEvent)ft5436_touchEvent[n];
	ft5436_Point point = {ft5436_touchX[n], ft5436_touchY[n]};

	switch (event) {

		case PressDown:
            ft5436_points[0] = point;
            ft5436_dragMode = false;
            // Note: Is in microseconds. Ref https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_timer.html
            ft5436_touchStartTime = esp_timer_get_time()/1000;
            ft5436_fireEvent(point, TouchStart);
			break;

		case Contact:
            // Dragging makes no sense IMHO. Since the X & Y are not getting updated while dragging
            // Dragging && _points[0].aboutEqual(point) - Not used IDEA 2:  && (lastEvent == 2)
            if (!ft5436_dragMode && 
                    (abs(ft5436_lastX-ft5436_touchX[n]) <= ft5436_maxDeviation || abs(ft5436_lastY-ft5436_touchY[n])<=ft5436_maxDeviation) && 
                    esp_timer_get_time()/1000 - ft5436_touchStartTime > 300) {
                ft5436_dragMode = true;
                ft5436_fireEvent(point, DragStart);
            } else if (ft5436_dragMode) {
                ft5436_fireEvent(point, DragMove);
            }
            ft5436_fireEvent(point, TouchMove);

            // For me the _touchStartTime shouold be set in both PressDown & Contact events, but after Drag detection
            ft5436_touchStartTime = esp_timer_get_time()/1000;
            break;

		case LiftUp:
			
			ft5436_points[9] = point;
			ft5436_touchEndTime = esp_timer_get_time()/1000;

			//printf("TIMEDIFF: %lu End: %lu\n", _touchEndTime - _touchStartTime, _touchEndTime);

			ft5436_fireEvent(point, TouchEnd);
			if (ft5436_dragMode) {
				ft5436_fireEvent(point, DragEnd);
				ft5436_dragMode = false;
			}
		
			if ( ft5436_touchEndTime - ft5436_touchStartTime <= 900) {
				// Do not get why this: _points[0].aboutEqual(point) (Original library)
				ft5436_fireEvent(point, Tap);
                ft5436_Point zeroPoint = {0, 0};
				ft5436_points[0] = zeroPoint;
				ft5436_touchStartTime = 0;
				ft5436_dragMode = false;
			}
			break;

        case NoEvent:
        break;
	}
	// Store lastEvent
	ft5436_lastEvent = (int) event;
	ft5436_lastX = ft5436_touchX[0];
	ft5436_lastY = ft5436_touchY[0];
}

void ft5436_xy_touch(ft5436_Point *point, uint8_t *count)
{
	ft5436_readData();

	if (point != NULL)
	{
		point->x = ft5436_touchX[0];
		point->y = ft5436_touchY[0];
	}

	*count = touch_cnt; 
}

void ft5436_xy_event(ft5436_Point *point, ft5436_Event *e)
{
	ft5436_readData();
	// TPoint point{_touchX[0], _touchY[0]};
	ft5436_RawEvent event = (ft5436_RawEvent)ft5436_touchEvent[0];
	ESP_LOGI(TAG, "Polled raw event: %d", event);

	if (point != NULL)
	{
		point->x = ft5436_touchX[0];
		point->y = ft5436_touchY[0];
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

static bool ft5436_readData(void)
{
    uint8_t data_size = 16U;     // Discarding last 2: 0x0E & 0x0F as not relevant
    uint8_t data[data_size];
    i2c_readRegister(dev_handle, FT6X36_REG_NUM_TOUCHES, &touch_cnt);

	i2c_master_receive(dev_handle, data, data_size, -1);
	
	/*
    ESP_LOGD(TAG, "Registers:");
    for (int16_t i = 0; i < data_size; i++)
    {
        ESP_LOGD(TAG, "%x:%hx", i, data[i]);
    }
	*/

    const uint8_t addrShift = 6;
	
	// READ X, Y and Touch events (X 2)
	for (uint8_t i = 0; i < 2; i++)
	{
		ft5436_touchEvent[i] = data[0 + i * addrShift] >> 6;
		ft5436_touchX[i] = data[1 + i * addrShift];
		ft5436_touchY[i] = data[3 + i * addrShift];
	}
	
	// Make _touchX[idx] and _touchY[idx] rotation aware
	switch (ft5436_rotation)
  {
	case 1:
	    ft5436_swap(&ft5436_touchX[0], &ft5436_touchY[0]);
		ft5436_swap(&ft5436_touchX[1], &ft5436_touchY[1]);
		ft5436_touchY[0] = ft5436_touch_width - ft5436_touchY[0] -1;
		ft5436_touchY[1] = ft5436_touch_width - ft5436_touchY[1] -1;
		break;
	case 2:
		ft5436_touchX[0] = ft5436_touch_width - ft5436_touchX[0] - 1;
		ft5436_touchX[1] = ft5436_touch_width - ft5436_touchX[1] - 1;
		ft5436_touchY[0] = ft5436_touch_height - ft5436_touchY[0] - 1;
		ft5436_touchY[1] = ft5436_touch_height - ft5436_touchY[1] - 1;
		break;
	case 3:
		ft5436_swap(&ft5436_touchX[0], &ft5436_touchY[0]);
		ft5436_swap(&ft5436_touchX[1], &ft5436_touchY[1]);
		ft5436_touchX[0] = ft5436_touch_height - ft5436_touchX[0] - 1;
		ft5436_touchX[1] = ft5436_touch_height - ft5436_touchX[1] - 1;
		break;
  }
	return true;
}

static void ft5436_fireEvent(ft5436_Point point, ft5436_Event e)
{
	if (ft5436_touchHandler)
		ft5436_touchHandler(point, e);
}

static void ft5436_debugInfo()
{
	ESP_LOGD(TAG,"TH_DIFF: %d CTRL: %d", i2c_getRegister8(dev_handle, FT6X36_REG_FILTER_COEF), i2c_getRegister8(dev_handle, FT6X36_REG_CTRL));
	ESP_LOGD(TAG,"TIMEENTERMONITOR: %d PERIODACTIVE: %d", i2c_getRegister8(dev_handle, FT6X36_REG_TIME_ENTER_MONITOR), i2c_getRegister8(dev_handle, FT6X36_REG_TOUCHRATE_ACTIVE));
	ESP_LOGD(TAG,"PERIODMONITOR: %d RADIAN_VALUE: %d", i2c_getRegister8(dev_handle, FT6X36_REG_TOUCHRATE_MONITOR), i2c_getRegister8(dev_handle, FT6X36_REG_RADIAN_VALUE));
	ESP_LOGD(TAG,"OFFSET_LEFT_RIGHT: %d OFFSET_UP_DOWN: %d", i2c_getRegister8(dev_handle, FT6X36_REG_OFFSET_LEFT_RIGHT), i2c_getRegister8(dev_handle, FT6X36_REG_OFFSET_UP_DOWN));
	ESP_LOGD(TAG,"DISTANCE_LEFT_RIGHT: %d DISTANCE_UP_DOWN: %d", i2c_getRegister8(dev_handle, FT6X36_REG_DISTANCE_LEFT_RIGHT), i2c_getRegister8(dev_handle, FT6X36_REG_DISTANCE_UP_DOWN));
	ESP_LOGD(TAG,"DISTANCE_ZOOM: %d CIPHER: %d", i2c_getRegister8(dev_handle, FT6X36_REG_DISTANCE_ZOOM), i2c_getRegister8(dev_handle, FT6X36_REG_CHIPID));
	ESP_LOGD(TAG," G_MODE: %d PWR_MODE: %d", i2c_getRegister8(dev_handle, FT6X36_REG_INTERRUPT_MODE), i2c_getRegister8(dev_handle, FT6X36_REG_POWER_MODE));
	ESP_LOGD(TAG," FIRMID: %d FOCALTECH_ID: %d STATE: %d", i2c_getRegister8(dev_handle, FT6X36_REG_FIRMWARE_VERSION), i2c_getRegister8(dev_handle, FT6X36_REG_PANEL_ID), i2c_getRegister8(dev_handle, FT6X36_REG_STATE));
}

static void ft5436_setRotation(uint8_t rotation) {
	ft5436_rotation = rotation;
}

static void ft5436_setTouchWidth(uint16_t width) {
	ESP_LOGI(TAG, "touch w:%d",width);
	ft5436_touch_width = width;
}

static void ft5436_setTouchHeight(uint16_t height) {
	ESP_LOGI(TAG, "touch h:%d\n",height);
	ft5436_touch_height = height;
}

static void ft5436_swap(uint16_t *a, uint16_t *b) {
    uint16_t *t = a;
    *a = *b;
    *b = *t;
}