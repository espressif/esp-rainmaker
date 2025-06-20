/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_err.h>
#include <esp_rmaker_core.h>

// Matter Commissioning Window Management service
#define ESP_RMAKER_SERVICE_MATTER_COMMISSIONING_WINDOW_MANAGEMENT "esp.service.matter-cwm"

// Matter Commissioning Window Management parameters
#define ESP_RMAKER_DEF_MATTER_QRCODE_NAME            "QRCode"
#define ESP_RMAKER_PARAM_MATTER_QRCODE               "esp.param.matter-qrcode"
#define ESP_RMAKER_DEF_MATTER_MANUALCODE_NAME        "ManualCode"
#define ESP_RMAKER_PARAM_MATTER_MANUALCODE           "esp.param.matter-manualcode"
#define ESP_RMAKER_DEF_MATTER_COMMISSIONING_WINDOW_OPEN_NAME     "WindowOpen"
#define ESP_RMAKER_PARAM_MATTER_COMMISSIONING_WINDOW_OPEN        "esp.param.window-open"

esp_rmaker_param_t *matter_commissioning_window_management_service_create(
    const char *serv_name, esp_rmaker_device_write_cb_t write_cb, esp_rmaker_device_read_cb_t read_cb, void *priv_data);
