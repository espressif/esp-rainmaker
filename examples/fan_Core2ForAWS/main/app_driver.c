/* Fan demo implementation using button and RGB LED

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <sdkconfig.h>

#include <iot_button.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h> 
#include <esp_rmaker_standard_params.h> 

#include <app_reset.h>
#include "app_priv.h"

#include <core2forAWS.h>
#include <lvgl/lvgl.h>

/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          0
#define BUTTON_ACTIVE_LEVEL  0
/* This is the GPIO on which the power will be set */
#define OUTPUT_GPIO    19

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10

static uint8_t g_speed = DEFAULT_SPEED;
static bool g_power = DEFAULT_POWER;

extern SemaphoreHandle_t xGuiSemaphore;
extern SemaphoreHandle_t spi_mutex;

static lv_obj_t * strength_slider;
static lv_obj_t *sw1;

esp_err_t app_fan_set_power(bool power)
{
    g_power = power;
    if (power) {
        Core2ForAWS_Motor_SetStrength(g_speed * 20);
        lv_switch_on(sw1, LV_ANIM_OFF);
    } else {
        Core2ForAWS_Motor_SetStrength(0);
        lv_switch_off(sw1, LV_ANIM_OFF);
    }
    return ESP_OK;
}

esp_err_t app_fan_set_speed(uint8_t speed)
{
    g_speed = speed;
    if (g_power) {
        Core2ForAWS_Motor_SetStrength(g_speed * 20);
    }
    lv_slider_set_value(strength_slider, g_speed, LV_ANIM_OFF);
    
    return 0;
}

static void strength_slider_event_cb(lv_obj_t * slider, lv_event_t event)
{

    if(event == LV_EVENT_VALUE_CHANGED) {
        app_fan_set_speed(lv_slider_get_value(slider));
    }
    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_type(fan_device, ESP_RMAKER_PARAM_SPEED),
            esp_rmaker_int(g_speed));
}

static void sw1_event_handler(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_VALUE_CHANGED) {
        app_fan_set_power(lv_switch_get_state(obj));
    }
    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_type(fan_device, ESP_RMAKER_PARAM_POWER),
            esp_rmaker_bool(g_power));
}

esp_err_t app_fan_init(void)
{
    spi_mutex = xSemaphoreCreateMutex();

    Core2ForAWS_Init();
    FT6336U_Init();
    Core2ForAWS_LCD_Init();
    Core2ForAWS_Button_Init();

    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    lv_obj_t * fan_state_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_set_pos(fan_state_label, 30, 25);
    lv_label_set_text(fan_state_label, "Fan On/Off:");

    sw1 = lv_switch_create(lv_scr_act(), NULL);
    lv_obj_set_size(sw1, 140, 30);
    lv_obj_align(sw1, fan_state_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
    lv_obj_set_event_cb(sw1, sw1_event_handler);
    lv_switch_on(sw1, LV_ANIM_OFF);

    lv_obj_t * fan_speed_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_align(fan_speed_label, sw1, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 25);
    lv_label_set_text(fan_speed_label, "Fan Speed:");

    strength_slider = lv_slider_create(lv_scr_act(), NULL);
    lv_obj_set_size(strength_slider, 260, 30);
    lv_obj_align(strength_slider, fan_speed_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
    lv_obj_set_event_cb(strength_slider, strength_slider_event_cb);
    lv_slider_set_value(strength_slider, 0, LV_ANIM_OFF);
    lv_slider_set_range(strength_slider, 0, 5);
    xSemaphoreGive(xGuiSemaphore);

    app_fan_set_speed(g_speed);
    app_fan_set_power(g_power);
    return ESP_OK;
}
static void initTask(void *arg)
{
    app_fan_init();
    vTaskDelete(NULL);
}

void app_driver_init()
{
    xTaskCreatePinnedToCore(initTask, "init_fan", 4096*2, NULL, 4, NULL, 1);
}
