/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Security version for on-network challenge-response service
 */
typedef enum {
    /** No security (not recommended for production) */
    ESP_RMAKER_ON_NETWORK_SEC_NONE = 0,
    /** Security 1: Curve25519 key exchange + AES-CTR encryption */
    ESP_RMAKER_ON_NETWORK_SEC1 = 1,
    /** Security 2: SRP6a key exchange + AES-GCM encryption */
    ESP_RMAKER_ON_NETWORK_SEC2 = 2
} esp_rmaker_on_network_sec_t;

/**
 * @brief Configuration structure for on-network challenge-response service
 */
typedef struct {
    /** HTTP server port (default: 80) */
    uint16_t port;

    /** Security version */
    esp_rmaker_on_network_sec_t sec_ver;

    /**
     * Proof of Possession for Security 1 (optional)
     * If NULL and sec_ver is SEC1, no POP will be required
     * Must remain valid throughout service lifetime
     */
    const char *pop;

    /**
     * Salt for Security 2 (required if sec_ver is SEC2)
     * Must remain valid throughout service lifetime
     */
    const uint8_t *salt;

    /** Length of salt */
    size_t salt_len;

    /**
     * Verifier for Security 2 (required if sec_ver is SEC2)
     * Must remain valid throughout service lifetime
     */
    const uint8_t *verifier;

    /** Length of verifier */
    size_t verifier_len;

    /** Enable mDNS service announcement for device discovery */
    bool enable_mdns;

    /**
     * mDNS instance name (optional)
     * If NULL, node_id will be used as instance name
     */
    const char *mdns_instance_name;
} esp_rmaker_on_network_chal_resp_config_t;

/**
 * @brief Default configuration for on-network challenge-response service
 */
#define ESP_RMAKER_ON_NETWORK_CHAL_RESP_DEFAULT_CONFIG() { \
    .port = 80, \
    .sec_ver = ESP_RMAKER_ON_NETWORK_SEC1, \
    .pop = NULL, \
    .salt = NULL, \
    .salt_len = 0, \
    .verifier = NULL, \
    .verifier_len = 0, \
    .enable_mdns = true, \
    .mdns_instance_name = NULL \
}

/**
 * @brief Initialize and start on-network challenge-response service
 *
 * This API:
 * 1. Creates a protocomm HTTP server instance
 * 2. Configures security (Sec0, Sec1, or Sec2) as specified
 * 3. Registers the challenge-response endpoint ("ch_resp")
 * 4. Optionally announces mDNS service for discovery
 *
 * @note Only one protocomm HTTP server can run at a time.
 *       This should not be used while WiFi provisioning is active.
 *
 * @param[in] config Configuration structure. If NULL, default config is used.
 *
 * @return
 *  - ESP_OK: Service started successfully
 *  - ESP_ERR_INVALID_ARG: Invalid configuration
 *  - ESP_ERR_NO_MEM: Memory allocation failed
 *  - ESP_ERR_INVALID_STATE: Service already started or protocomm conflict
 *  - ESP_FAIL: Failed to start service
 */
esp_err_t esp_rmaker_on_network_chal_resp_start(
    const esp_rmaker_on_network_chal_resp_config_t *config);

/**
 * @brief Stop on-network challenge-response service
 *
 * This API:
 * 1. Stops the protocomm HTTP server
 * 2. Removes mDNS service announcement (if enabled)
 * 3. Cleans up resources
 *
 * @return
 *  - ESP_OK: Service stopped successfully
 *  - ESP_ERR_INVALID_STATE: Service not started
 */
esp_err_t esp_rmaker_on_network_chal_resp_stop(void);

/**
 * @brief Check if on-network challenge-response service is running
 *
 * @return true if service is running, false otherwise
 */
bool esp_rmaker_on_network_chal_resp_is_running(void);

/**
 * @brief Disable challenge-response for on-network service
 *
 * This function disables the challenge-response functionality while keeping
 * the HTTP server running. Once disabled:
 * 1. The ch_resp handler will return "Disabled" status for any requests
 * 2. mDNS service is removed (if enabled)
 *
 * This is useful after a successful user-node mapping to prevent further
 * mapping attempts.
 *
 * @return
 *  - ESP_OK: Successfully disabled
 *  - ESP_ERR_INVALID_STATE: Service not running
 */
esp_err_t esp_rmaker_on_network_chal_resp_disable(void);

/**
 * @brief Re-enable challenge-response for on-network service
 *
 * This function re-enables the challenge-response functionality and
 * re-announces the mDNS service (if it was enabled). This can be called
 * after disabling challenge-response to make it available again.
 *
 * @return
 *  - ESP_OK: Successfully re-enabled
 *  - ESP_ERR_INVALID_STATE: Service not running
 */
esp_err_t esp_rmaker_on_network_chal_resp_enable(void);

#ifdef __cplusplus
}
#endif

