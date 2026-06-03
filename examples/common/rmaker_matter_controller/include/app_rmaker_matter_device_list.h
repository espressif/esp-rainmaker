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

#define ESP_MATTER_DEVICE_MAX_ENDPOINT CONFIG_RAINMAKER_MATTER_CONTROLLER_MAX_ENDPOINT_COUNT_PER_DEVICE
#define ESP_MATTER_DEVICE_NAME_MAX_LEN 32
#define ESP_RAINMAKER_NODE_ID_MAX_LEN 36
#define ESP_MATTER_IPK_LEN 16

typedef struct endpoint_entry {
    uint16_t endpoint_id;
    uint32_t device_type_id;
    char device_name[ESP_MATTER_DEVICE_NAME_MAX_LEN];
} endpoint_entry_t;

typedef struct matter_device {
    uint64_t node_id;
    char rainmaker_node_id[ESP_RAINMAKER_NODE_ID_MAX_LEN];
    bool reachable;
    bool is_rainmaker_device;
    bool is_metadata_fetched;
    uint8_t endpoint_count;
    endpoint_entry_t endpoints[ESP_MATTER_DEVICE_MAX_ENDPOINT];
    struct matter_device *next;
} matter_device_t;

#ifdef __cplusplus
}
#endif
