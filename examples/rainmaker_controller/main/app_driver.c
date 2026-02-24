/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Board driver: button for Wi-Fi reset and factory reset */

#include <sdkconfig.h>
#include <esp_log.h>

#include <iot_button.h>
#include <button_gpio.h>

#include <app_reset.h>

#include "app_priv.h"

#define BUTTON_GPIO                 CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL         0
#define WIFI_RESET_BUTTON_TIMEOUT  3
#define FACTORY_RESET_BUTTON_TIMEOUT 10

void app_driver_init()
{
    button_config_t btn_cfg = {
        .long_press_time = 0,
        .short_press_time = 0,
    };
    button_gpio_config_t gpio_cfg = {
        .gpio_num = BUTTON_GPIO,
        .active_level = BUTTON_ACTIVE_LEVEL,
        .enable_power_save = false,
    };
    button_handle_t btn_handle = NULL;
    if (iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn_handle) == ESP_OK && btn_handle) {
        /* Register Wi-Fi reset and factory reset on same button */
        app_reset_button_register(btn_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
    }
}
