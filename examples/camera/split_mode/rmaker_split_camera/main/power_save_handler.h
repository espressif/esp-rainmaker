/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __POWER_SAVE_HANDLER_H__
#define __POWER_SAVE_HANDLER_H__

#include "esp_err.h"

/**
 * @brief Initialize power save handling for split mode operation
 *
 * This function:
 * - Initializes host power save monitoring with callbacks
 * - Initializes slave light sleep component
 * - Registers WebRTC event handler for waking host on offer/streaming events
 *
 * Should be called after eh_cp_init() and before app_webrtc_init()
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_FAIL: Initialization failed
 */
esp_err_t power_save_init(void);

/**
 * @brief Hook the WebRTC events that wake the host
 *
 * Separate from power_save_init() because it needs app_webrtc, which comes up
 * later. Call it after app_webrtc_init().
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_FAIL: Callback registration failed
 */
esp_err_t power_save_register_webrtc_events(void);


#endif /* __POWER_SAVE_HANDLER_H__ */
