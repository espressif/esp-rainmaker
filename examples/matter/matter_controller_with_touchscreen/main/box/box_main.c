/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "box_main.h"
#include "box_platform.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "ui_main.h"
#include <stdio.h>

static const char *TAG = "box_main";

void box_main(void)
{
    ESP_LOGI(TAG, "Compile time: %s %s", __DATE__, __TIME__);
    ESP_ERROR_CHECK(box_platform_init());
    ESP_LOGI(TAG, "Board: %s", box_platform_get_name());

    ESP_LOGI(TAG, "Display LVGL demo");
    ESP_ERROR_CHECK(ui_main_start());

    ESP_LOGI(TAG, "Current Free Memory\t%d\t SPIRAM:%d\n",
             heap_caps_get_free_size(MALLOC_CAP_8BIT) - heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
