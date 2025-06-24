/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <app_matter_controller.h>

esp_err_t app_matter_controller_callback(matter_controller_handle_t *handle, matter_controller_callback_type_t type);
