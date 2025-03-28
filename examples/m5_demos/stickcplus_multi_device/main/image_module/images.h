/*
* SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
* 
* SPDX-License-Identifier: Apache-2.0
*/
#pragma once
#include "M5Unified.h"
#include "interface_module/m5_interface.h"
#include "lvgl_port_m5stack.hpp"

extern lv_obj_t *img_rainmaker_icon;

extern lv_obj_t *bulb_circle;
extern lv_obj_t *base_circle;
extern lv_obj_t *line;

/**
 * RainMaker logo icon image
 */
void display_rainmaker_icon(void);
void init_rainmaker_icon(void);
void display_rainmaker_icon(void);

/**
 * Light bulb symbol
 */
void display_light_bulb(void);
void display_light_bulb_on(void);
void display_light_bulb_off(void);

void init_images(void);