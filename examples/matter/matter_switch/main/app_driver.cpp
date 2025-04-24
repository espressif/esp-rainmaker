/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include <device.h>
#include <button_gpio.h>
#include <esp_matter.h>
#include <led_driver.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <app_matter_switch.h>
#include <app_priv.h>

static const char *TAG = "app_driver";
extern uint16_t switch_endpoint_id;
static bool g_power = DEFAULT_POWER;

/* Do any conversions/remapping for the actual value here */
esp_err_t app_driver_switch_set_power(led_driver_handle_t handle, bool val)
{
    g_power = val;
    return led_driver_set_power(handle, val);
}

static void app_driver_button_toggle_cb(void *handle, void *usr_data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
    app_matter_send_command_binding(!g_power);
}

esp_err_t app_driver_light_set_defaults()
{
    return app_driver_switch_set_power((led_driver_handle_t)esp_matter::endpoint::get_priv_data(switch_endpoint_id),
                                        DEFAULT_POWER);
}

app_driver_handle_t app_driver_light_init()
{
    /* Initialize led */
    led_driver_config_t config = led_driver_get_config();
    led_driver_handle_t handle = led_driver_init(&config);
    return (app_driver_handle_t)handle;
}

app_driver_handle_t app_driver_button_init(void *user_data)
{
    /* Initialize button */
    button_handle_t handle = NULL;
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = button_driver_get_config();

    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
        return NULL;
    }

    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, app_driver_button_toggle_cb, user_data);
    return (app_driver_handle_t)handle;
}
