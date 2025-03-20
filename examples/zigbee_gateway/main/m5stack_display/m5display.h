/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_log.h>
#include <esp_event.h>

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_M5STACK_ZIGBEE_GATEWAY_BOARD
void app_m5stack_display_start();
void app_register_m5stack_display_event_handler();

ESP_EVENT_DECLARE_BASE(M5STACK_DISPLAY_EVENT);

typedef enum {
   DISPLAY_ZIGBEE_PANID_EVENT = 1,
   DISPLAY_ZIGBEE_DEVICE_COUNT_EVENT,
   DISPLAY_ZIGBEE_ADD_DEVICE_STATE,
} app_m5stack_display_event_t;

#endif

#ifdef __cplusplus
}
#endif
