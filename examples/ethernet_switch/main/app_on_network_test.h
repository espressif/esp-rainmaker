/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the on-network challenge-response test
 *
 * This function registers event handlers for IP and WiFi events to
 * automatically start/stop the on-network challenge-response service.
 *
 * Call this function from your app_main() after app_network_init().
 *
 * @return ESP_OK on success, an error code otherwise.
 */
esp_err_t app_on_network_chal_resp_init(void);

#ifdef __cplusplus
}
#endif

