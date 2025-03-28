/*
* SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
* 
* SPDX-License-Identifier: Apache-2.0
*/
#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "button_module/m5_button.h"
#include "image_module/images.h"
#include "neohex_module/m5_neohex.h"
#include "relay_module/m5_relay.h"

#include "app_priv.h"

extern lv_obj_t *start_scrn_bg;
extern lv_obj_t *start_btn;
extern lv_obj_t *start_header;
extern lv_style_t start_btn_style;

extern lv_obj_t *sgl_sw_scrn_bg;
extern lv_obj_t *sgl_sw;
extern lv_obj_t *sgl_sw_header;
extern lv_obj_t *bulb_symbol;

extern lv_obj_t *light_scrn_bg;
extern lv_obj_t *light_sw;
extern lv_obj_t *light_header;
extern lv_obj_t *brt_slider;
extern lv_obj_t *hue_slider;

extern lv_obj_t *relay_scrn_bg;
extern lv_obj_t *relay_header;
extern lv_obj_t *relay_sw_1;
extern lv_obj_t *relay_sw_2;
extern lv_obj_t *relay_sw_3;
extern lv_obj_t *relay_sw_4;

extern bool is_start_scrn;
extern bool is_sgl_sw_scrn;
extern bool is_light_scrn;
extern bool is_relay_scrn;
extern bool is_wifi_reset;
extern bool is_factory_reset;

void init_m5(void);
void set_start_scrn(void);
void set_sgle_sw_scrn(void);
void set_light_scrn(void);
void set_relay_sw_scrn(void);
void set_wifi_reset_notice(void);
void set_fctry_reset_notice(void);

extern "C" 
{
void del_prov_display(void);

void display_wifi_rst(void);
void display_fctry_rst(void);

void show_start_screen(void);
void display_start_screen(void);
void display_start_btn_pressed(void);
void reset_start_btn(void);

void display_sgl_switch_screen(void);
void show_sgl_switch_screen(void);

void display_light_screen(void);
void show_light_scrn(void);

void display_relay_screen(void);
void show_relay_scrn(void);

void display_qrcode_m5(const char *payload);
void display_pop_m5(const char *pop);
}
