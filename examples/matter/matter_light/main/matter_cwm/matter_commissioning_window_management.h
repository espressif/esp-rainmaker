/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <stdbool.h>
#include <esp_err.h>

esp_err_t matter_commissioning_window_parameters_update();
esp_err_t matter_commissioning_window_status_update(bool open);

esp_err_t matter_commissioning_window_management_enable();
