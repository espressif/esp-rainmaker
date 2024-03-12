/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "sdkconfig.h"
#include "choose_bsp.h"

void printf_choose_bsp()
{
#if CONFIG_BSP_BOARD_ESP32_S3_BOX_3
    ESP_LOGW("choose_bsp", "select board: box-3");
#endif

#if CONFIG_BSP_BOARD_ESP32_S3_BOX
    ESP_LOGW("choose_bsp", "select board: box");
#endif

#if CONFIG_BSP_BOARD_ESP32_S3_BOX_Lite
    ESP_LOGW("choose_bsp", "select board: box-lite");
#endif

#if CONFIG_BSP_BOARD_ESP32_S3_LCD_EV_BOARD
    ESP_LOGW("choose_bsp", "select board: s3-lcd-ev-board");
#endif
}
