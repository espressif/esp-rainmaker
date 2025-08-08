/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Initialize Challenge Response
 *
 * This initializes the challenge response module.
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_chal_resp_init(void);

/** Deinitialize Challenge Response
 *
 * This deinitializes the challenge response module.
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_chal_resp_deinit(void);

#ifdef __cplusplus
}
#endif
