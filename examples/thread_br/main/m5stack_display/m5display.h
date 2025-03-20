/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_log.h>

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_M5STACK_THREAD_BR_BOARD
void app_m5stack_display_start();
void app_register_m5stack_display_event_handler();
#endif

#ifdef __cplusplus
}
#endif
