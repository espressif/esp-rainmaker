/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <esp_err.h>
#include <esp_event.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Ethernet driver
 *
 * This initializes the Ethernet driver and network interface
 */
esp_err_t app_ethernet_init(void);

/**
 * @brief Start Ethernet connection
 *
 * This will start the Ethernet driver and wait for connection.
 * Function will return successfully only after Ethernet is connected and IP is obtained.
 *
 * @return ESP_OK on success (Ethernet connected and IP obtained).
 * @return error in case of failure.
 */
esp_err_t app_ethernet_start(void);

/**
 * @brief Stop Ethernet connection
 *
 * This will stop the Ethernet driver.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t app_ethernet_stop(void);

#ifdef __cplusplus
}
#endif
