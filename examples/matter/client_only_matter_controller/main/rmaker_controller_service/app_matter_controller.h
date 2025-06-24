/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_err.h>
#include <esp_rmaker_core.h>
#include <stdint.h>

#include <lib/support/Span.h>

typedef union {
    int raw;
    struct {
        unsigned int base_url_set: 1;
        unsigned int user_token_set: 1;
        unsigned int access_token_set: 1;
        unsigned int rmaker_group_id_set: 1;
        unsigned int matter_fabric_id_set: 1;
        unsigned int matter_node_id_set: 1;
        unsigned int matter_noc_installed: 1;
    };
} matter_controller_status_t;

typedef struct {
    char *base_url;
    char *user_token;
    char *access_token;
    char *rmaker_group_id;
    uint64_t matter_fabric_id;
    uint64_t matter_node_id;
    bool matter_noc_installed;
    uint16_t matter_vendor_id;
    esp_rmaker_device_t *service;
} matter_controller_handle_t;

typedef enum {
    MATTER_CONTROLLER_CALLBACK_TYPE_AUTHORIZE = 1,
    MATTER_CONTROLLER_CALLBACK_TYPE_QUERY_MATTER_FABRIC_ID,
    MATTER_CONTROLLER_CALLBACK_TYPE_SETUP_CONTROLLER,
    MATTER_CONTROLLER_CALLBACK_TYPE_UPDATE_CONTROLLER_NOC,
    MATTER_CONTROLLER_CALLBACK_TYPE_UPDATE_DEVICE,
} matter_controller_callback_type_t;

typedef esp_err_t (*matter_controller_callback_t)(matter_controller_handle_t *handle, matter_controller_callback_type_t type);

esp_err_t matter_controller_handle_update();

esp_err_t matter_controller_enable(uint16_t matter_vendor_id, matter_controller_callback_t callback);

esp_err_t matter_controller_report_status(matter_controller_status_t status);
