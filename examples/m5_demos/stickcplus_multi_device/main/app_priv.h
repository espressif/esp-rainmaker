/*!
 * @copyright This example code is in the Public Domain (or CC0 licensed, at your option.)

 * Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#pragma once
#include "Arduino.h"

extern "C" 
{
#include <app_reset.h>
#include <sdkconfig.h>

#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_utils.h>

#include "lv_qrcode_private.h"
#include <app_network.h>
#include <nvs_flash.h>
}

#include "interface_module/m5_interface.h"

#define DEFAULT_SWITCH_POWER false
#define DEFAULT_RM_HUE 180
#define DEFAULT_RM_BRIGHTNESS 25

#define PARAM_TYPE "param_type"
#define PARAM_NAME "param_name"

#define ON_OFF_SW 0
#define BRT_SW 1
#define HUE_SW 2
#define COLOR_SKY_BLUE 0xABE9FF
#define COLOR_ORANGE 0xFFA500
#define COLOR_WHITE 0xFFFFFF
#define COLOR_GREEN 0x00FF00
#define COLOR_GREY 0xD3D3D3
#define COLOR_RED 0xFF0000
#define COLOR_CRIMSON 0xC70039

#define RELAY_SW_1 0
#define RELAY_SW_2 1
#define RELAY_SW_3 2
#define RELAY_SW_4 3

#define WIFI_RESET_BUTTON_TIMEOUT 5000
#define FACTORY_RESET_BUTTON_TIMEOUT 8000
#define HOME_RETURN_TIMEOUT 3000
#define SW_DIRECTION_UP true
#define SW_DIRECTION_DOWN false

#define REBOOT_DELAY 2
#define RESET_DELAY 2
#define EVENT_DEBOUNCE_INTERVAL 300 // milliseconds

extern bool rm_sgl_sw_state;
extern bool rm_light_sw_state;
extern bool rm_relay_sw_state[4];

extern bool light_sw_array[3];
extern int8_t brt_level;
extern int16_t hue_level;
extern bool relay_sw_array[4];

extern esp_rmaker_device_t *switch_device;
extern esp_rmaker_device_t *light_device;
extern esp_rmaker_device_t *relay_device;

void update_rm_param(const char *param_attr, const char *param_id, esp_rmaker_device_t *device,
                     esp_rmaker_param_val_t val);
void init_app_driver(void);
void deinit_i2c(void);
void scrn_event_cb(lv_event_t *event);

void start_btn_cb(lv_event_t *event);

void my_single_switch_cb(lv_event_t *event);
void set_sgl_sw_state(bool state);

void light_sw_select_cb(lv_event_t *event);
void light_sw_cb(lv_event_t *event);
void light_brt_cb(lv_event_t *event);
void light_hue_cb(lv_event_t *event);
bool get_light_sw_status(int call_sw);
void set_light_sw_state(bool state);
void set_light_brt(int brt_value);
void set_light_hue(int hue_value);
void set_sw(int call_sw);

void relay_sw_select_cb(lv_event_t *event);
void relay_sw_cb(lv_event_t *event);
void set_relay_sw_state(void);
void set_relay_sw(int call_sw);
void disc_relay(void);