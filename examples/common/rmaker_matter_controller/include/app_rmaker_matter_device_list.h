/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MATTER_DEVICE_MAX_ENDPOINT CONFIG_RMAKER_MTCTL_MAX_ENDPOINT_COUNT_PER_DEVICE
#define ESP_MATTER_DEVICE_MAX_DEVICE_TYPE CONFIG_RMAKER_MTCTL_MAX_DEVICE_TYPE_COUNT_PER_ENDPOINT
#define ESP_MATTER_DEVICE_NAME_MAX_LEN CONFIG_RMAKER_MTCTL_DEVICE_NAME_MAX_LEN
#define ESP_RAINMAKER_NODE_ID_MAX_LEN CONFIG_RMAKER_MTCTL_RMAKER_NODE_ID_MAX_LEN
#define ESP_MATTER_IPK_LEN 16

typedef struct endpoint_entry {
    uint16_t endpoint_id;
    uint8_t device_type_count;
    uint32_t device_type_list[ESP_MATTER_DEVICE_MAX_DEVICE_TYPE];
} endpoint_entry_t;

typedef struct matter_device {
    uint64_t node_id;
    char device_name[ESP_MATTER_DEVICE_NAME_MAX_LEN];
    char rainmaker_node_id[ESP_RAINMAKER_NODE_ID_MAX_LEN];
    bool is_rainmaker_device;
    uint8_t endpoint_count;
    endpoint_entry_t endpoints[ESP_MATTER_DEVICE_MAX_ENDPOINT];
    struct matter_device *next;
} matter_device_t;

/**
 * @brief Check whether the controller has enough state to update the Matter device list.
 *
 * @return true if the base URL, user token, group id, authorization, and controller setup are ready
 * @return false if any required controller state is missing
 */
bool app_rmaker_matter_device_list_updatable(void);

/**
 * @brief Update the Matter device list from RainMaker cloud.
 *
 * The fetched list is passed to matter_controller_device_list_update_callback_t as a temporary read-only list. The
 * callback must copy it if it needs to keep it after returning.
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_matter_device_list_update(void);

/**
 * @brief Deep-copy a Matter device list.
 *
 * The returned list prefers PSRAM when CONFIG_RMAKER_MTCTL_MEMORY_ALLOCATION_PREFER_SPIRAM is enabled.
 *
 * @param[in] src_dev_list The Matter device list to copy
 *
 * @return Copied Matter device list on success
 * @return NULL if allocation failed or src_dev_list is NULL
 */
matter_device_t *app_rmaker_device_list_copy_create(const matter_device_t *src_dev_list);

/**
 * @brief Destroy a list returned by app_rmaker_device_list_copy_create() or fetched by the controller API.
 *
 * @param[in] dev_list The Matter device list to destroy
 */
void app_rmaker_device_list_copy_destroy(matter_device_t *dev_list);

/**
 * @brief Print a Matter device list.
 *
 * @param[in] dev_list The Matter device list to print
 */
void app_rmaker_device_list_print(const matter_device_t *dev_list);

#ifdef __cplusplus
}
#endif
