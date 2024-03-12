/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "box_main.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_style_t style;
    lv_style_t style_focus_no_outline;
    lv_style_t style_focus;
    lv_style_t style_pr;
} button_style_t;

#ifdef CONFIG_BSP_BOARD_ESP32_S3_LCD_EV_BOARD
#include "bsp/esp32_s3_lcd_ev_board.h"
#define UI_SCALING(x) (x * 2)
#define UI_PAGE_H_RES (uint16_t)(BSP_LCD_H_RES * 0.375)
#else
#define UI_SCALING(x) (x * 1)
#define UI_PAGE_H_RES 290
#endif

esp_err_t ui_main_start(void);
void ui_acquire(void);
void ui_release(void);
lv_group_t *ui_get_btn_op_group(void);
button_style_t *ui_button_styles(void);
lv_obj_t *ui_main_get_status_bar(void);
void ui_main_status_bar_set_wifi(bool is_connected);
void ui_main_status_bar_set_cloud(bool is_connected);

#ifdef __cplusplus
}
#endif
