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
 * - Applies the Wi-Fi power-save mode at each station start
 * - Initializes host power save monitoring with callbacks
 * - Initializes the co-processor light sleep controller
 *
 * app_main() calls it after app_network_init() and before app_network_start().
 * The WebRTC event handler is registered separately, by
 * power_save_register_webrtc_events().
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_FAIL: Initialization failed
 */
esp_err_t power_save_init(void);

/**
 * @brief Hook the WebRTC event that wakes the host when media starts
 *
 * An offer wakes the host in the signaling bridge adapter, not here.
 * app_webrtc_register_event_callback() creates its lock on first use, so this
 * can run before app_webrtc_init(); app_main() calls it before
 * rmaker_webrtc_camera_init().
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_FAIL: Callback registration failed
 */
esp_err_t power_save_register_webrtc_events(void);


#endif /* __POWER_SAVE_HANDLER_H__ */
