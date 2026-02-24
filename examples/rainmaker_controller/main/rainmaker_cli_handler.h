/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* RainMaker CLI handler API (User API wrapper) */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

/**
 * @brief Initialize the RainMaker CLI handler
 *
 * @param refresh_token Refresh token for the RainMaker API
 * @param base_url Base URL for the RainMaker API
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t rainmaker_cli_handler_init(const char *refresh_token, const char *base_url);

/**
 * @brief Get nodes list from the RainMaker cloud, nodes list need to be freed by the caller
 *
 * @param nodes_list Pointer to the nodes list
 * @param nodes_count Pointer to the nodes count
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t rainmaker_cli_handler_get_nodes_list(char **nodes_list, uint16_t *nodes_count);

/**
 * @brief Get node details from the RainMaker cloud, node details need to be freed by the caller
 *
 * @param node_details Pointer to the node details
 * @param status_code Pointer to the status code
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t rainmaker_cli_handler_get_node_details(char **node_details, int *status_code);

/**
 * @brief Get node params by node id from the RainMaker cloud, node params need to be freed by the caller
 *
 * @param node_id Node ID
 * @param node_params Pointer to the node params
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t rainmaker_cli_handler_get_node_params(const char *node_id, char **node_params);

/**
 * @brief Set node parameters by node id from the RainMaker cloud, response data need to be freed by the caller
 *
 * @param node_id Node ID
 * @param payload JSON string containing parameter updates
 * @param response_data Pointer to the response data
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t rainmaker_cli_handler_set_node_params(const char *node_id, const char *payload, char **response_data);

/**
 * @brief Get node config by node id from the RainMaker cloud, node config need to be freed by the caller
 *
 * @param node_id Node ID
 * @param node_config Pointer to the node config
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t rainmaker_cli_handler_get_node_config(const char *node_id, char **node_config);

/**
 * @brief Get node connection status by node id from the RainMaker cloud
 *
 * @param node_id Node ID
 * @param connection_status Pointer to store the connection status (true = online)
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t rainmaker_cli_handler_get_node_connection_status(const char *node_id, bool *connection_status);

/**
 * @brief Remove node by node id from the RainMaker cloud, response data need to be freed by the caller
 *
 * @param node_id Node ID
 * @param response_data Pointer to the response data
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t rainmaker_cli_handler_remove_node(const char *node_id, char **response_data);
