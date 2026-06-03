/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <nvs.h>
#include <stdint.h>
#include <string.h>

#include "app_rmaker_matter_controller_internal.h"

#define TAG "rmaker_matter_controller_nvs"

esp_err_t rmaker_matter_controller_get_nvs(const char *key, uint8_t *val_buf, size_t *val_len)
{
    if (!key || !val_len) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(MATTER_CTL_NVS_PART_NAME, MATTER_CTL_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    // First get the required size
    size_t required_size = 0;
    err = nvs_get_blob(handle, key, NULL, &required_size);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }
    if (required_size == 0) {
        nvs_erase_key(handle, key);
        nvs_commit(handle);
        nvs_close(handle);
        return ESP_ERR_NOT_FOUND;
    }
    // Check if buffer is large enough
    if (*val_len < required_size) {
        *val_len = required_size;
        nvs_close(handle);
        return ESP_ERR_INVALID_SIZE;
    }
    if (!val_buf) {
        return ESP_ERR_INVALID_ARG;
    }
    // Get the actual data
    err = nvs_get_blob(handle, key, val_buf, val_len);
    nvs_close(handle);
    return err;
}

esp_err_t rmaker_matter_controller_set_nvs(const char *key, const uint8_t *val, size_t val_len)
{
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(MATTER_CTL_NVS_PART_NAME, MATTER_CTL_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(handle, key, val, val_len);
    nvs_commit(handle);
    nvs_close(handle);
    return err;
}
