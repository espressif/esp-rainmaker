/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "box_main.h"
#include "bsp_board.h"
#include "bsp_storage.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "ui_main.h"
#include <stdio.h>
#include "bsp/esp-bsp.h"

static const char *TAG = "box_main";

void box_main(void)
{
    ESP_LOGI(TAG, "Compile time: %s %s", __DATE__, __TIME__);
    bsp_i2c_init();
    bsp_display_start();
    bsp_board_init();
    ESP_LOGI(TAG, "Display LVGL demo");
    bsp_display_backlight_on();
    ESP_ERROR_CHECK(ui_main_start());

    ESP_LOGI(TAG, "Current Free Memory\t%d\t SPIRAM:%d\n",
             heap_caps_get_free_size(MALLOC_CAP_8BIT) - heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
