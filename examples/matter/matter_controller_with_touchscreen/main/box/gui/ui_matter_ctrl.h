/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include "app_matter_ctrl.h"
#include "esp_err.h"
#include "ui_main.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_MATTER_EVT_LOADING,
    UI_MATTER_EVT_START_COMMISSION,
    UI_MATTER_EVT_COMMISSIONCOMPLETE,
    UI_MATTER_EVT_FAILED_COMMISSION,
    UI_MATTER_EVT_REFRESH,
} ui_matter_state_t;

void ui_matter_ctrl_start(void (*fn)(void));
void ui_matter_config_update_cb(ui_matter_state_t state);
void clean_screen_with_button();
void ui_set_onoff_state(lv_obj_t *g_func_btn, size_t size_type, bool state);

#ifdef __cplusplus
}
#endif
