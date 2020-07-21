/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <esp_err.h>

/** Types of Proof of Possession */
typedef enum {
    /** Use MAC address to generate PoP */
    POP_TYPE_MAC,
    /** Use random stream generated and stored in fctry partition during claiming process as PoP */
    POP_TYPE_RANDOM
} app_wifi_pop_type_t;

void app_wifi_init();
esp_err_t app_wifi_start(app_wifi_pop_type_t pop_type);
