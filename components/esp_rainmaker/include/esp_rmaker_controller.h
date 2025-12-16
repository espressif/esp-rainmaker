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
extern "C"
{
#endif

/** Function prototype for Controller Callback
 *
 * This function will be invoked by the ESP RainMaker Controller whenever a node data is received.
 * Please do not block in this callback.
 *
 * @param[in] node_id The node ID.
 * @param[in] data The data received from the node.
 * @param[in] data_size The size of the data received from the node.
 * @param[in] priv_data The private data passed in esp_rmaker_controller_enable().
 */
typedef esp_err_t (*esp_rmaker_controller_cb_t) (const char *node_id, const char *data, size_t data_size, void *priv_data);

/** Controller Configuration
 *
 * This structure contains the configuration for the controller service.
 *
 * @param[in] report_node_details Whether to report the node details when the controller service is enabled.
 * @param[in] cb The callback function to be invoked when a node data is received.
 * @param[in] priv_data The private data to be passed to the callback function.
 */
typedef struct {
    bool report_node_details;
    esp_rmaker_controller_cb_t cb;
    void *priv_data;
} esp_rmaker_controller_config_t;

/** Create the controller service
 *
 * This function creates the controller service.
 *
 * @param[in] config The configuration for the controller service.
 *
 * @return ESP_OK on success.
 * @return ESP_FAIL on failure.
 */
esp_err_t esp_rmaker_controller_enable(esp_rmaker_controller_config_t *config);

/** Disable the controller service
 *
 * This function disables the controller service.
 *
 * @return ESP_OK on success.
 * @return ESP_FAIL on failure.
 */
esp_err_t esp_rmaker_controller_disable(void);

/** Get the active group ID
 *
 * This function gets the active group ID.
 *
 * @return The active group ID.
 * @return NULL if the active group ID is not set.
 */
char *esp_rmaker_controller_get_active_group_id(void);

/** Get the user token
 *
 * This function gets the user token.
 *
 * @return The user token.
 * @return NULL if the user token is not set.
 */
char *esp_rmaker_controller_get_user_token(void);

/** Get the base URL
 *
 * This function gets the base URL.
 *
 * @return The base URL.
 * @return NULL if the base URL is not set.
 */
char *esp_rmaker_controller_get_base_url(void);

#ifdef __cplusplus
}
#endif
