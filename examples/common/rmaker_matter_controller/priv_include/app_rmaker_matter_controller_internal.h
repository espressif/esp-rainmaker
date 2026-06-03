/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <esp_rmaker_core.h>
#include <freertos/FreeRTOS.h>
#include <stdbool.h>
#include <stdint.h>

#include <app_rmaker_matter_controller.h>

#ifdef __cplusplus
extern "C" {
#endif

// The NVS partition name for rainmaker matter controller should be same as ESP_RMAKER_NVS_PART_NAME defined in
// esp_rmaker_internal.h, which is a private header file, so we define it here.
#define MATTER_CTL_NVS_PART_NAME "nvs"
#define MATTER_CTL_NVS_NAMESPACE "rm-matter-ctl"
#define MATTER_CTL_NVS_KEY_NOC "ctl-noc"
#define MATTER_CTL_NVS_KEY_KEYPAIR "ctl-kp"
#define MATTER_CTL_NVS_KEY_RCAC "ctl-rcac"

typedef enum {
    MATTER_CONTROLLER_EVENT_TYPE_AUTHORIZE = 1,
    MATTER_CONTROLLER_EVENT_TYPE_SETUP_CONTROLLER,
    MATTER_CONTROLLER_EVENT_TYPE_UPDATE_CONTROLLER_NOC,
    MATTER_CONTROLLER_EVENT_TYPE_UPDATE_DEVICE_LIST,
    MATTER_CONTROLLER_EVENT_TYPE_UPDATE_HANDLE,
} matter_controller_event_type_t;

typedef struct {
    char *base_url;
    char *user_token;
    char *rmaker_group_id;
    QueueHandle_t event_task_queue;
    TaskHandle_t event_task_handle;
    SemaphoreHandle_t dev_list_mutex;
    matter_device_t *dev_list;
    device_list_update_callback_t dev_list_update_cb;
    bool is_setup_successfully_before;
    bool is_authorized;
    bool is_controller_setup;
    bool is_server_instance;
    matter_controller_setup_callback_t setup_callback;
    matter_controller_update_noc_callback_t update_noc_callback;
    esp_rmaker_device_t *service;
    uint64_t matter_node_id;
} matter_controller_handle_t;

typedef union {
    int raw;
    struct {
        unsigned int base_url_set : 1;
        unsigned int user_token_set : 1;
        unsigned int rmaker_group_id_set : 1;
        unsigned int is_setup_successfully_before : 1;
        unsigned int matter_case_permission : 1;
    };
} matter_controller_status_t;

/**
 * @brief Get the value from the NVS partition for rainmaker matter controller
 *
 * @param[in] key The key to get the value from
 * @param[out] val_buf The buffer to store the value
 * @param[in,out] val_len The length of buffer as input and the length of the got value as output
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t rmaker_matter_controller_get_nvs(const char *key, uint8_t *val_buf, size_t *val_len);

/**
 * @brief Set the value to the NVS partition for rainmaker matter controller
 *
 * @param[in] key The key to set the value to
 * @param[in] val The value to set
 * @param[in] val_len The length of the value
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t rmaker_matter_controller_set_nvs(const char *key, const uint8_t *val, size_t val_len);

#ifdef __cplusplus
}
#endif
