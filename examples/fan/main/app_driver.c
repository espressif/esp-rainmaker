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
#include <ws2812_led.h>
#include "app_priv.h"

/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0

#define DEFAULT_HUE         180
#define DEFAULT_SATURATION  100
#define DEFAULT_BRIGHTNESS  ( 20 * DEFAULT_SPEED)

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10

static uint8_t g_speed = DEFAULT_SPEED;
static uint16_t g_hue = DEFAULT_HUE;
static uint16_t g_saturation = DEFAULT_SATURATION;
static uint16_t g_value = DEFAULT_BRIGHTNESS;
static bool g_power = DEFAULT_POWER;

esp_err_t app_fan_set_power(bool power)
{
    g_power = power;
    if (power) {
        ws2812_led_set_hsv(g_hue, g_saturation, g_value);
    } else {
        ws2812_led_clear();
    }
    return ESP_OK;
}

esp_err_t app_fan_set_speed(uint8_t speed)
{
    g_speed = speed;
    g_value = 20 * g_speed;
    return ws2812_led_set_hsv(g_hue, g_saturation, g_value);
}

esp_err_t app_fan_init(void)
{
    esp_err_t err = ws2812_led_init();
    if (err != ESP_OK) {
        return err;
    }
    app_fan_set_power(g_power);
    return ESP_OK;
}

static void push_btn_cb(void *arg)
{
    uint8_t old_speed = g_speed;
    g_speed++;
    if (g_speed > 5) {
        g_speed = 0;
    }
    app_fan_set_speed(g_speed);
    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_type(fan_device, ESP_RMAKER_PARAM_SPEED),
            esp_rmaker_int(g_speed));
    if (old_speed == 0) {
        g_power = true;
        esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(fan_device, ESP_RMAKER_PARAM_POWER),
                esp_rmaker_bool(g_power));
    } else if (g_speed == 0) {
        g_power = false;
        esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(fan_device, ESP_RMAKER_PARAM_POWER),
                esp_rmaker_bool(g_power));
    }
}

void app_driver_init()
{
    app_fan_init();
    button_handle_t btn_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, push_btn_cb, NULL);
        /* Register Wi-Fi reset and factory reset functionality on same button */
        app_reset_button_register(btn_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
    }
}
