/* Simple GPIO Demo
   
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <sdkconfig.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <esp_log.h>

#include <iot_button.h>

#include "app_priv.h"

#define RMT_TX_CHANNEL RMT_CHANNEL_0
/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          0
#define BUTTON_ACTIVE_LEVEL  0
/* This is the GPIO on which the power will be set */

#define OUTPUT_GPIO_RED   16ULL
#define OUTPUT_GPIO_GREEN 14ULL
#define OUTPUT_GPIO_BLUE  15ULL

esp_err_t app_driver_set_gpio(const char *name, bool state)
{
    if (strcmp(name, "Red") == 0) {
        gpio_set_level(OUTPUT_GPIO_RED, state);
    } else if (strcmp(name, "Green") == 0) {
        gpio_set_level(OUTPUT_GPIO_GREEN, state);
    } else if (strcmp(name, "Blue") == 0) {
        gpio_set_level(OUTPUT_GPIO_BLUE, state);
    } else {
        return ESP_FAIL;
    }
    return ESP_OK;
}
static void button_press_3sec_cb(void *arg)
{
    nvs_flash_deinit();
    nvs_flash_erase();
    esp_restart();
}

void app_driver_init()
{
    button_handle_t btn_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
        iot_button_add_on_press_cb(btn_handle, 3, button_press_3sec_cb, NULL);
    }

    /* Configure power */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };
    uint64_t pin_mask = (((uint64_t)1 << OUTPUT_GPIO_RED ) | ((uint64_t)1 << OUTPUT_GPIO_GREEN ) | ((uint64_t)1 << OUTPUT_GPIO_BLUE ));
    io_conf.pin_bit_mask = pin_mask;
    /* Configure the GPIO */
    gpio_config(&io_conf);
    gpio_set_level(OUTPUT_GPIO_RED, false);
    gpio_set_level(OUTPUT_GPIO_GREEN, false);
    gpio_set_level(OUTPUT_GPIO_BLUE, false);
}
