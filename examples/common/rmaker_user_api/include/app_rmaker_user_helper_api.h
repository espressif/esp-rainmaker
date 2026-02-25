/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_ENABLE_RM_USER_HELPER_API
/**
 * @brief Node mapping operation types
 */
typedef enum {
    /* Add node mapping. */
    APP_RMAKER_USER_HELPER_API_ADD_NODE_MAPPING = 0,
    /* Remove node mapping. */
    APP_RMAKER_USER_HELPER_API_REMOVE_NODE_MAPPING,
} app_rmaker_user_helper_api_node_mapping_operation_type_t;

/**
 * @brief Node mapping status types
 */
typedef enum {
    /* Node mapping status requested. */
    APP_RMAKER_USER_HELPER_API_NODE_MAPPING_STATUS_REQUESTED = 0,
    /* Node mapping status confirmed. */
    APP_RMAKER_USER_HELPER_API_NODE_MAPPING_STATUS_CONFIRMED,
    /* Node mapping status timeout. */
    APP_RMAKER_USER_HELPER_API_NODE_MAPPING_STATUS_TIMEOUT,
    /* Node mapping status discarded. */
    APP_RMAKER_USER_HELPER_API_NODE_MAPPING_STATUS_DISCARDED,
    /* Node mapping status internal error. */
    APP_RMAKER_USER_HELPER_API_NODE_MAPPING_STATUS_INTERNAL_ERROR,
} app_rmaker_user_helper_api_node_mapping_status_type_t;

/**
 * @brief Helper function to get user ID.
 *
 * This function retrieves the user ID associated with the RainMaker account.
 *
 * @param user_id Pointer to store user ID, caller must free when return ESP_OK
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_helper_api_get_user_id(char **user_id);

/**
 * @brief Helper function to get nodes list associated with the account.
 *
 * This function retrieves all nodes (devices) associated with the RainMaker account.
 *
 * @param nodes_list Pointer to store nodes list, caller must free when return ESP_OK
 * @param nodes_count Pointer to store nodes count
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_helper_api_get_nodes_list(char **nodes_list, uint16_t *nodes_count);

/**
 * @brief Helper function to get node config.
 *
 * This function retrieves the config of a node (device) in the RainMaker cloud.
 *
 * @param node_id Node ID
 * @param node_config Pointer to store node config, caller must free
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_helper_api_get_node_config(const char *node_id, char **node_config);

/**
 * @brief Helper function to set node parameters.
 *
 * This function updates the parameters of a node or multiple nodes (device) in the RainMaker cloud.
 * The parameters should be provided as a JSON string.
 *
 * @param node_id Node ID for single node, NULL for multiple nodes
 * @param payload JSON string containing parameter updates; if node_id is NULL, the payload should
 *                be a JSON string containing parameter updates for multiple nodes
 * @param response_data Pointer to store response data, caller must free
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_helper_api_set_node_params(const char *node_id, const char *payload, char **response_data);

/**
 * @brief Helper function to get node parameters.
 *
 * This function retrieves the current parameters of a specific node (device).
 *
 * @param node_id Node ID for specific node
 * @param node_params Pointer to store node params, caller must free
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_helper_api_get_node_params(const char *node_id, char **node_params);

/**
 * @brief Helper function to get groups.
 *
 * This function retrieves groups associated with the RainMaker account.
 * If group_id is NULL, all groups will be returned.
 *
 * @param group_id ID of the group to get
 * @param groups Pointer to store groups, caller must free
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_helper_api_get_groups(const char *group_id, char **groups);

/**
 * @brief Helper function to create a new group.
 *
 * This function creates a new group in the RainMaker cloud.
 *
 * @param group_payload JSON string containing group creation details
 * @param response_data Pointer to store response data, caller must free
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_helper_api_create_group(const char *group_payload, char **response_data);

/**
 * @brief Helper function to delete a group.
 *
 * This function deletes an existing group from the RainMaker cloud.
 *
 * @param group_id ID of the group to delete
 * @param response_data Pointer to store response data, caller must free
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_helper_api_delete_group(const char *group_id, char **response_data);

/**
 * @brief Helper function to add or remove node from group.
 *
 * This function adds or removes a node (device) from a group.
 *
 * @param group_id ID of the target group
 * @param group_payload JSON string containing group operation details
 * @param response_data Pointer to store response data, caller must free
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_helper_api_operate_node_to_group(const char *group_id, const char *group_payload, char **response_data);

/**
 * @brief Helper function to set node mapping.
 *
 * This function sets the node mapping for a user.
 *
 * @param secret_key Secret key; not required when removing
 * @param node_id Node ID
 * @param operation_type Operation type (app_rmaker_user_helper_api_node_mapping_operation_type_t)
 * @param request_id Pointer to store request ID, caller must free
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_helper_api_set_node_mapping(const char *secret_key, const char *node_id,
                                              app_rmaker_user_helper_api_node_mapping_operation_type_t operation_type,
                                              char **request_id);

/**
 * @brief Helper function to get node mapping status.
 *
 * This function retrieves the status of a node mapping request.
 *
 * @param request_id Request ID
 * @param status Pointer to store node mapping status
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_helper_api_get_node_mapping_status(const char *request_id,
                                                     app_rmaker_user_helper_api_node_mapping_status_type_t *status);

/**
 * @brief Helper function to get node connection status.
 *
 * This function retrieves the connection status of a node.
 *
 * @param node_id Node ID
 * @param connection_status Pointer to store connection status
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_helper_api_get_node_connection_status(const char *node_id, bool *connection_status);
#endif

#ifdef __cplusplus
}
#endif
