/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_rmaker_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Channel name prefix for KVS WebRTC */
#define ESP_RMAKER_CHANNEL_NAME_PREFIX "esp-v1-"

/* RainMaker device and parameter definitions */
#define ESP_RMAKER_DEF_CHANNEL_NAME "Channel"
#define ESP_RMAKER_DEVICE_CAMERA "esp.device.camera"
#define ESP_RMAKER_PARAM_CHANNEL "esp.param.channel"

/**
 * @brief Optional initialization callback function type
 *
 * This callback is called during rmaker_webrtc_camera_init() to allow
 * mode-specific initialization (e.g., work queue setup, sleep command registration).
 *
 * @param user_data User data passed during config setup
 * @return 0 on success, non-zero on failure
 */
typedef int (*rmaker_webrtc_camera_init_callback_t)(void *user_data);

/**
 * @brief Configuration structure for WebRTC camera initialization
 */
typedef struct {
    void *peer_connection_if;                          /*!< Required: Peer connection interface implementation (opaque handle) */
    void *video_capture;                                /*!< Optional: Video capture interface (NULL for signaling-only mode) */
    void *audio_capture;                                /*!< Optional: Audio capture interface (NULL for signaling-only mode) */
    void *video_player;                                 /*!< Optional: Video player interface (NULL for signaling-only mode) */
    void *audio_player;                                 /*!< Optional: Audio player interface (NULL for signaling-only mode) */
    rmaker_webrtc_camera_init_callback_t init_callback; /*!< Optional: Mode-specific initialization callback */
    void *init_callback_user_data;                      /*!< Optional: User data passed to init_callback */
} rmaker_webrtc_camera_config_t;

/**
 * @brief Create a RainMaker camera device with name and channel parameters
 *
 * This function creates a camera device and automatically adds the name
 * parameter and channel parameter (based on node ID) to it.
 *
 * @param dev_name Device name
 * @param priv_data Private data to associate with the device
 * @return Pointer to the created device, or NULL on failure
 */
esp_rmaker_device_t *esp_rmaker_camera_device_create(const char *dev_name, void *priv_data);

/**
 * @brief Initialize and start the WebRTC stack
 *
 * This function initializes the WebRTC stack with RainMaker credentials
 * and starts the WebRTC application in a separate task.
 *
 * @param config Configuration structure containing peer connection and media interfaces
 */
void rmaker_webrtc_camera_init(rmaker_webrtc_camera_config_t *config);

#ifdef __cplusplus
}
#endif
