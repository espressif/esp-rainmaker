/*
* SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
* 
* SPDX-License-Identifier: Apache-2.0
*/
#include "images.h"

lv_obj_t *bulb_circle = NULL;
lv_obj_t *base_circle = NULL;
lv_obj_t *line = NULL;

void display_light_bulb(void) 
{
    if (bulb_circle == NULL && base_circle == NULL && line == NULL) {
        // Bulb circle
        bulb_circle = lv_obj_create(sgl_sw_scrn_bg);
        lv_obj_set_size(bulb_circle, 50, 50);
        lv_obj_set_style_radius(bulb_circle, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(bulb_circle, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(bulb_circle, LV_ALIGN_CENTER, 0, -20);

        // Base circle
        base_circle = lv_obj_create(sgl_sw_scrn_bg);
        lv_obj_set_size(base_circle, 20, 10);
        lv_obj_set_style_radius(base_circle, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(base_circle, lv_color_hex(0x808080), 0);
        lv_obj_align_to(base_circle, bulb_circle, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    }
}

void display_light_bulb_on(void) 
{ 
  lv_obj_set_style_bg_color(bulb_circle, lv_color_hex(0xFFFF00), 0); 
}

void display_light_bulb_off(void) 
{ 
  lv_obj_set_style_bg_color(bulb_circle, lv_color_hex(0xFFFFFF), 0); 
}