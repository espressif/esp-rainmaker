/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/* It is recommended to copy this code in your example so that you can modify as
 * per your application's needs, especially for the indicator callbacks,
 * wifi_reset_indicate() and factory_reset_indicate().
 */
#include <esp_log.h>
#include <esp_err.h>
#include <driver/gpio.h>
#include <iot_button.h>
#include <button_gpio.h>
#include <esp_rmaker_utils.h>

static const char *TAG = "app_reset";

#define REBOOT_DELAY        2
#define RESET_DELAY         2

static void wifi_reset_trigger(void *arg, void *data)
{
    esp_rmaker_wifi_reset(RESET_DELAY, REBOOT_DELAY);
}

static void wifi_reset_indicate(void *arg, void *data)
{
    ESP_LOGI(TAG, "Release button now for Wi-Fi reset. Keep pressed for factory reset.");
}

static void factory_reset_trigger(void *arg, void *data)
{
    esp_rmaker_factory_reset(RESET_DELAY, REBOOT_DELAY);
}

static void factory_reset_indicate(void *arg, void *data)
{
    ESP_LOGI(TAG, "Release button to trigger factory reset.");
}

esp_err_t app_reset_button_register(button_handle_t btn_handle, uint8_t wifi_reset_timeout,
        uint8_t factory_reset_timeout)
{
    if (!btn_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ESP_OK;

    if (wifi_reset_timeout) {
        /* Register callback for Wi-Fi reset on button release after holding for wifi_reset_timeout seconds */
        button_event_args_t wifi_reset_args = {
            .long_press.press_time = wifi_reset_timeout * 1000,
        };
        ret |= iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_UP, &wifi_reset_args, wifi_reset_trigger, NULL);

        /* Register callback to indicate Wi-Fi reset after holding for wifi_reset_timeout seconds */
        button_event_args_t wifi_indicate_args = {
            .long_press.press_time = wifi_reset_timeout * 1000,
        };
        ret |= iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_START, &wifi_indicate_args, wifi_reset_indicate, NULL);
    }

    if (factory_reset_timeout) {
        if (factory_reset_timeout <= wifi_reset_timeout) {
            ESP_LOGW(TAG, "It is recommended to have factory_reset_timeout > wifi_reset_timeout");
        }

        /* Register callback for factory reset on button release after holding for factory_reset_timeout seconds */
        button_event_args_t factory_reset_args = {
            .long_press.press_time = factory_reset_timeout * 1000,
        };
        ret |= iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_UP, &factory_reset_args, factory_reset_trigger, NULL);

        /* Register callback to indicate factory reset after holding for factory_reset_timeout seconds */
        button_event_args_t factory_indicate_args = {
            .long_press.press_time = factory_reset_timeout * 1000,
        };
        ret |= iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_START, &factory_indicate_args, factory_reset_indicate, NULL);
    }

    return ret;
}

button_handle_t app_reset_button_create(gpio_num_t gpio_num, uint8_t active_level)
{
    button_config_t btn_cfg = {
        .long_press_time = 0,  /* Use default */
        .short_press_time = 0, /* Use default */
    };
    button_gpio_config_t gpio_cfg = {
        .gpio_num = gpio_num,
        .active_level = active_level,
        .enable_power_save = false,
    };
    button_handle_t btn_handle = NULL;
    if (iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn_handle) != ESP_OK) {
        return NULL;
    }
    return btn_handle;
}
