/*!
 * @copyright This example code is in the Public Domain (or CC0 licensed, at your option.)

 * Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#pragma once
#include "interface_module/m5_interface.h"
#include "lvgl_port_m5stack.hpp"
#include <FastLED.h>

#define Neopixel_PIN 32
#define NUM_LEDS 37
#define NUM_LEDS_LIGHT_DEVICE 19

#define DEFAULT_BRT 30
#define FULL_BRT 95
#define NULL_BRT 5
#define DEFAULT_SAT 255
#define DEFAULT_HUE 120
#define MAX_HUE 355
#define MIN_HUE 5

#define PURPLE_COLOR 191
#define GREEN_COLOR 120

extern CRGB leds[NUM_LEDS];

extern "C" 
{
void init_neohex(void);
void set_brt(void);
void set_hue(void);
void set_lighting_state(bool state);
}