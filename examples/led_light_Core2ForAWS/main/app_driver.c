/*  LED Lightbulb demo implementation using RGB LED

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
#include <core2forAWS.h>
#include <lvgl/lvgl.h>
#include "app_priv.h"

static uint16_t g_hue = DEFAULT_HUE;
static uint16_t g_saturation = DEFAULT_SATURATION;
static uint16_t g_value = DEFAULT_BRIGHTNESS;
static bool g_power = DEFAULT_POWER;

extern SemaphoreHandle_t xGuiSemaphore;
extern SemaphoreHandle_t spi_mutex;
static lv_obj_t * brightness_slider;
static lv_obj_t * hue_slider;
static lv_obj_t * saturation_slider;
static lv_obj_t *sw1;
/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
static void hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}


esp_err_t app_light_set_led(uint32_t hue, uint32_t saturation, uint32_t brightness)
{
    /* Whenever this function is called, light power will be ON */
    if (!g_power) {
        return 0;
    }
    uint32_t r, g, b, color;
    hsv2rgb(hue, saturation, brightness, &r, &g, &b);
    for (int i = 0; i < 10; i++) {
        r = r & 0xff; g = g & 0xff; b = b & 0xff;
        color = 0;
        color = (r << 24) | (g << 16) | (b << 8);
    }
    Core2ForAWS_Sk6812_SetSideColor(0, color);
    Core2ForAWS_Sk6812_SetSideColor(1, color);
    Core2ForAWS_Sk6812_Show();
    return 0;
}

esp_err_t app_light_set_power(bool power)
{
    g_power = power;
    if (power) {
        app_light_set_led(g_hue, g_saturation, g_value);
        lv_switch_on(sw1, LV_ANIM_OFF);
    } else {
        Core2ForAWS_Sk6812_Clear();
        Core2ForAWS_Sk6812_Show();
        lv_switch_off(sw1, LV_ANIM_OFF);
    }
    return ESP_OK;
}

esp_err_t app_light_set_brightness(uint16_t brightness)
{
    g_value = brightness;
    lv_slider_set_value(brightness_slider, g_value, LV_ANIM_OFF);
    return app_light_set_led(g_hue, g_saturation, g_value);
}
esp_err_t app_light_set_hue(uint16_t hue)
{
    g_hue = hue;
    lv_slider_set_value(hue_slider, g_hue, LV_ANIM_OFF);
    return app_light_set_led(g_hue, g_saturation, g_value);
}
esp_err_t app_light_set_saturation(uint16_t saturation)
{
    g_saturation = saturation;
    lv_slider_set_value(saturation_slider, g_saturation, LV_ANIM_OFF);
    return app_light_set_led(g_hue, g_saturation, g_value);
}

static void slider_event_cb(lv_obj_t * slider, lv_event_t event)
{
    if (event != LV_EVENT_RELEASED) {
        return;
    }

    if (slider == brightness_slider) {
        g_value = lv_slider_get_value(slider);
        esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_type(light_device, ESP_RMAKER_PARAM_BRIGHTNESS),
            esp_rmaker_int(g_value));
    } else if (slider == hue_slider) {
        g_hue = lv_slider_get_value(slider);
        esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_type(light_device, ESP_RMAKER_PARAM_HUE),
            esp_rmaker_int(g_hue));
    } else if (slider == saturation_slider) {
        g_saturation = lv_slider_get_value(slider);
        esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_type(light_device, ESP_RMAKER_PARAM_SATURATION),
            esp_rmaker_int(g_saturation));
    }
    app_light_set_led(g_hue, g_saturation, g_value);
}

static void sw1_event_handler(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_VALUE_CHANGED) {
        app_light_set_power(lv_switch_get_state(obj));
    } else {
        return;
    }

    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_type(light_device, ESP_RMAKER_PARAM_POWER),
            esp_rmaker_bool(g_power));
}


esp_err_t app_light_init(void)
{
    spi_mutex = xSemaphoreCreateMutex();

    Core2ForAWS_Init();
    FT6336U_Init();
    Core2ForAWS_LCD_Init();
    Core2ForAWS_Button_Init();
    Core2ForAWS_Sk6812_Init();

    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    lv_obj_t * light_state_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_set_pos(light_state_label, 30, 25);
    lv_label_set_text(light_state_label, "Light On/Off:");

    sw1 = lv_switch_create(lv_scr_act(), NULL);
    lv_obj_set_size(sw1, 140, 30);
    lv_obj_align(sw1, light_state_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
    lv_obj_set_event_cb(sw1, sw1_event_handler);
    lv_switch_on(sw1, LV_ANIM_OFF);

    lv_obj_t * light_hsv_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_align(light_hsv_label, sw1, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 25);
    lv_label_set_text(light_hsv_label, "Light Brightness, Hue, Saturation:");

    brightness_slider = lv_slider_create(lv_scr_act(), NULL);
    lv_obj_set_size(brightness_slider, 260, 20);
    lv_obj_align(brightness_slider, light_hsv_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    lv_obj_set_event_cb(brightness_slider, slider_event_cb);
    lv_slider_set_value(brightness_slider, 0, LV_ANIM_OFF);
    lv_slider_set_range(brightness_slider, 0, 100);

    hue_slider = lv_slider_create(lv_scr_act(), NULL);
    lv_obj_set_size(hue_slider, 260, 20);
    lv_obj_align(hue_slider, brightness_slider, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    lv_obj_set_event_cb(hue_slider, slider_event_cb);
    lv_slider_set_value(hue_slider, 0, LV_ANIM_OFF);
    lv_slider_set_range(hue_slider, 0, 360);

    saturation_slider = lv_slider_create(lv_scr_act(), NULL);
    lv_obj_set_size(saturation_slider, 260, 20);
    lv_obj_align(saturation_slider, hue_slider, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    lv_obj_set_event_cb(saturation_slider, slider_event_cb);
    lv_slider_set_value(saturation_slider, 0, LV_ANIM_OFF);
    lv_slider_set_range(saturation_slider, 0, 100);


    xSemaphoreGive(xGuiSemaphore);
    app_light_set_hue(g_hue);
    app_light_set_saturation(g_saturation);
    app_light_set_brightness(g_value);

    return ESP_OK;
}

static void initTask(void *arg)
{
    app_light_init();
    vTaskDelete(NULL);
}


void app_driver_init()
{
    xTaskCreatePinnedToCore(initTask, "init_light", 4096*2, NULL, 4, NULL, 1);
}
