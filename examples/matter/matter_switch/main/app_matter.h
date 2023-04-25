/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_err.h>
#include <app_priv.h>

esp_err_t app_matter_init();
esp_err_t app_matter_switch_create(app_driver_handle_t driver_handle);
esp_err_t app_matter_start();
esp_err_t app_matter_pre_rainmaker_start();
esp_err_t app_matter_send_command_binding(bool power);
void app_matter_enable_matter_console();
