/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <esp_rmaker_ota.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_ESP_RMAKER_OTA_USE_HTTPS

/**
 * @brief HTTPS OTA callback function
 *
 * This function handles HTTPS OTA update with retry logic and status reporting.
 *
 * @param[in] ota_handle The OTA handle
 * @param[in] ota_data The OTA data containing URL and other information
 *
 * @return ESP_OK on success
 * @return ESP_FAIL on failure
 */
esp_err_t esp_rmaker_ota_https_cb(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data);

#endif /* CONFIG_ESP_RMAKER_OTA_USE_HTTPS */

#ifdef __cplusplus
}
#endif
