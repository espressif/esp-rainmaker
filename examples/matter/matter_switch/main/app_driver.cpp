/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <sdkconfig.h>
#include <stdlib.h>
#include <string.h>

#include <app_end_device.h>
#include <esp_matter.h>
#include <iot_button.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <app_matter_switch.h>
#include <app_priv.h>

static const char *TAG = "app_driver";
extern uint16_t switch_endpoint_id;
static bool g_power = DEFAULT_POWER;

esp_err_t app_driver_switch_set_power(app_driver_handle_t handle, bool val)
{
    led_indicator_handle_t led = static_cast<led_indicator_handle_t>(handle);
    uint8_t level = val ? UINT8_MAX : 0;
    esp_err_t err = app_end_device_led_set_rgb(led, level, level, level);
    if (err == ESP_OK) {
        g_power = val;
    }
    return err;
}

static void app_driver_button_toggle_cb(void *handle, void *usr_data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
    app_matter_send_command_binding(!g_power);
}

esp_err_t app_driver_light_set_defaults()
{
    return app_driver_switch_set_power(esp_matter::endpoint::get_priv_data(switch_endpoint_id), DEFAULT_POWER);
}

app_driver_handle_t app_driver_light_init()
{
    ESP_LOGI(TAG, "Initializing switch indicator");
    return static_cast<app_driver_handle_t>(app_end_device_led_init());
}

app_driver_handle_t app_driver_button_init(void *user_data)
{
    button_handle_t handle = app_end_device_button_init();
    if (!handle) {
        return NULL;
    }

    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, app_driver_button_toggle_cb, user_data);
    return (app_driver_handle_t)handle;
}
