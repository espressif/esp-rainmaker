/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <esp_err.h>

/** Enable Connectivity Service
 *
 * This API enables the connectivity service for the node. It creates a
 * "Connectivity" service with a "Connected" parameter that indicates
 * the MQTT connection status.
 *
 * When enabled:
 * - On MQTT connect: Reports {"Connectivity":{"Connected":true}}
 * - Sets MQTT LWT to publish {"Connectivity":{"Connected":false}} on disconnect
 * - LWT topic is set to node/{node_id}/params/local or
 *   node/{node_id}/params/local/{group_id} if group_id is set
 * - Automatically reads persisted group_id from flash and configures LWT accordingly
 *
 * @note This API MUST be called after esp_rmaker_node_init() but before esp_rmaker_start().
 *       The Connected parameter is initialized with the current MQTT connection status and
 *       will be automatically updated when MQTT connects or disconnects.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_connectivity_enable(void);

/** Update Connectivity LWT for Group
 *
 * Updates the LWT topic to include the group_id. This should be called
 * when the group_id changes. The LWT topic will be:
 * - node/{node_id}/params/local if group_id is NULL or empty
 * - node/{node_id}/params/local/{group_id} if group_id is set
 *
 * @note This is called automatically by esp_rmaker_register_for_group_params()
 * if connectivity service is enabled.
 *
 * @param[in] group_id The group ID to use in the LWT topic. Can be NULL.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_connectivity_update_lwt(const char *group_id);

/** Check if Connectivity Service is enabled
 *
 * @return true if connectivity service is enabled.
 * @return false otherwise.
 */
bool esp_rmaker_connectivity_is_enabled(void);

#ifdef __cplusplus
}
#endif
