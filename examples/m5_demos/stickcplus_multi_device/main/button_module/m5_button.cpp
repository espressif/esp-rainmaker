/*
* SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
* 
* SPDX-License-Identifier: Apache-2.0
*/
#include "m5_button.h"

static TickType_t btnB_press_strt_time = 0;
static TickType_t btnA_press_strt_time = 0;
static bool is_prev_BtnA_pressed = false;

static bool is_wifi_reset_timeout(void) 
{
    return !is_wifi_reset &&
           (xTaskGetTickCount() - btnB_press_strt_time) >= pdMS_TO_TICKS(WIFI_RESET_BUTTON_TIMEOUT);
}

static bool is_fctry_reset_timeout(void) 
{
    return (is_wifi_reset && !is_factory_reset &&
            ((xTaskGetTickCount() - btnB_press_strt_time) >= pdMS_TO_TICKS(FACTORY_RESET_BUTTON_TIMEOUT)));
}

static void check_BtnA_status(void) 
{
    if (is_start_scrn && M5.BtnA.wasPressed()) {
        is_prev_BtnA_pressed = true;
    }
}

static void pressing_btnA(void) 
{
    if (is_prev_BtnA_pressed && M5.BtnA.isPressed()) {
        if (is_start_scrn) {
            lv_obj_send_event(start_btn, LV_EVENT_CLICKED, NULL);
        }
    }
}

static void released_btnA(void) 
{
    if (M5.BtnA.wasReleased()) {
        if (is_start_scrn && is_prev_BtnA_pressed) {
            lv_obj_send_event(start_btn, LV_EVENT_RELEASED, NULL);
            is_prev_BtnA_pressed = false;
        } else if (is_sgl_sw_scrn) {
            lv_obj_send_event(sgl_sw, LV_EVENT_CLICKED, NULL);
        } else if (is_light_scrn && light_sw_array[ON_OFF_SW]) {
            lv_obj_send_event(light_sw, LV_EVENT_CLICKED, NULL);
        } else if (is_light_scrn && light_sw_array[BRT_SW]) {
            lv_obj_send_event(brt_slider, LV_EVENT_CLICKED, NULL);
        } else if (is_light_scrn && light_sw_array[HUE_SW]) {
            lv_obj_send_event(hue_slider, LV_EVENT_CLICKED, NULL);
        } else if (is_relay_scrn) {
            lv_obj_send_event(relay_sw_1, LV_EVENT_CLICKED, NULL);
        }
    }
}

static void longPress_btnA(void) 
{
    if (M5.BtnA.wasPressed()) {
        btnA_press_strt_time = xTaskGetTickCount();
    }
    if (M5.BtnA.isPressed()) {
        if ((xTaskGetTickCount() - btnA_press_strt_time) >= pdMS_TO_TICKS(HOME_RETURN_TIMEOUT)) {
            lv_obj_send_event(sgl_sw_header, LV_EVENT_PRESSING, NULL);
            lv_obj_send_event(light_header, LV_EVENT_PRESSING, NULL);
            btnA_press_strt_time = xTaskGetTickCount();
        }
    }
}

static void holding_btnA(void) 
{
    if (M5.BtnA.wasHold()) {
        if (is_light_scrn && light_sw_array[BRT_SW]) {
            lv_obj_send_event(brt_slider, LV_EVENT_LONG_PRESSED, NULL);
        } else if (is_light_scrn && light_sw_array[HUE_SW]) {
            lv_obj_send_event(hue_slider, LV_EVENT_LONG_PRESSED, NULL);
        }
    }
}

static void released_after_hold_btnA(void) 
{
    if (is_start_scrn && M5.BtnA.wasReleasedAfterHold()) {
        lv_obj_send_event(start_btn, LV_EVENT_REFRESH, NULL);
    }
}

static void longPress_btnB(void) 
{
    if (M5.BtnB.wasPressed()) {
        btnB_press_strt_time = xTaskGetTickCount();
    }
    if (M5.BtnB.isPressed()) {
        if (is_wifi_reset_timeout()) {
            set_wifi_reset_notice();
        } else if (is_fctry_reset_timeout()) {
            set_fctry_reset_notice();
            btnB_press_strt_time = xTaskGetTickCount();
        }
    }
}

static void released_btnB(void) 
{
    if (M5.BtnB.wasReleased()) {
        if (is_wifi_reset) {
            lv_obj_send_event(start_header, LV_EVENT_LONG_PRESSED, NULL);
        } else if (is_factory_reset) {
            lv_obj_send_event(start_header, LV_EVENT_LONG_PRESSED, NULL);
        } else if (is_light_scrn && !is_wifi_reset && !is_factory_reset) {
            lv_obj_send_event(light_header, LV_EVENT_SHORT_CLICKED, NULL);
        } else if (is_relay_scrn && !is_wifi_reset && !is_factory_reset) {
            lv_obj_send_event(relay_header, LV_EVENT_SHORT_CLICKED, NULL);
        }
    }
}

static void press_double_btnB(void) 
{
    if (M5.BtnB.wasDoubleClicked()) {
        if (is_sgl_sw_scrn) {
            lv_obj_send_event(sgl_sw_header, LV_EVENT_PRESSED, NULL);
        } else if (is_light_scrn) {
            lv_obj_send_event(light_header, LV_EVENT_PRESSED, NULL);
        } else if (is_relay_scrn) {
            lv_obj_send_event(relay_header, LV_EVENT_PRESSED, NULL);
        }
    }
}

void check_button_events(void) 
{
    M5.update();
    check_BtnA_status();
    pressing_btnA();
    released_btnA();
    holding_btnA();
    longPress_btnA();
    released_after_hold_btnA();
    longPress_btnB();
    released_btnB();
    press_double_btnB();
    vTaskDelay(10 / portTICK_PERIOD_MS); // Small delay to reset watchdog.
}
