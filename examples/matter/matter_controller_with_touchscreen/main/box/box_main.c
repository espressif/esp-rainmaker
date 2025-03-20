/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "box_main.h"
#include "choose_bsp.h"
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
    printf_choose_bsp();
    bsp_i2c_init();
#if CONFIG_BSP_BOARD_ESP32_S3_LCD_EV_BOARD
    bsp_display_cfg_t cfg;
#else
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * 10,
        .double_buffer = 0,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
        }
    };
    cfg.lvgl_port_cfg.task_affinity = 1;
#endif
    bsp_display_start_with_config(&cfg);

    ESP_LOGI(TAG, "Display LVGL demo");
    bsp_display_backlight_on();
    ESP_ERROR_CHECK(ui_main_start());

    ESP_LOGI(TAG, "Current Free Memory\t%d\t SPIRAM:%d\n",
             heap_caps_get_free_size(MALLOC_CAP_8BIT) - heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
