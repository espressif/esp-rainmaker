/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_err.h>
#include <esp_matter.h>

esp_err_t app_matter_init(esp_matter::attribute::callback_t app_attribute_update_cb, esp_matter::identification::callback_t app_identification_cb);
esp_err_t app_matter_start(esp_matter::event_callback_t app_event_cb);
esp_err_t app_matter_rmaker_init();
esp_err_t app_matter_rmaker_start();
void app_matter_enable_matter_console();