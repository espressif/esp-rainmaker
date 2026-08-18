/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cJSON.h>
#include <esp_rmaker_core.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set the RainMaker MTDevices parameter used for attribute reports
 *
 * @param[in] matter_devices_param The RainMaker parameter handle for MTDevices
 */
void app_rmaker_matter_report_set_param(esp_rmaker_param_t *matter_devices_param);

/**
 * @brief Update a Matter attribute JSON tree from a cJSON value
 *
 * @param[in,out] root The per-node Matter attribute JSON root
 * @param[in] endpoint_id The Matter endpoint ID
 * @param[in] cluster_id The Matter cluster ID
 * @param[in] attribute_id The Matter attribute ID
 * @param[in] value The read-only cJSON attribute value
 *
 * @return true if the tree was updated
 * @return false if the input was invalid or allocation failed
 */
bool app_rmaker_matter_report_json_update_tree_item(cJSON *root, uint16_t endpoint_id, uint32_t cluster_id,
                                                   uint32_t attribute_id, const cJSON *value);

/**
 * @brief Check whether a Matter attribute JSON tree contains an equal value
 *
 * @param[in] root The per-node Matter attribute JSON root
 * @param[in] endpoint_id The Matter endpoint ID
 * @param[in] cluster_id The Matter cluster ID
 * @param[in] attribute_id The Matter attribute ID
 * @param[in] value The read-only cJSON attribute value
 *
 * @return true if the stored value is semantically equal
 * @return false if the value differs or the path is absent
 */
bool app_rmaker_matter_report_json_tree_item_matches(const cJSON *root, uint16_t endpoint_id, uint32_t cluster_id,
                                                     uint32_t attribute_id, const cJSON *value);

/**
 * @brief Publish a Matter devices delta to RainMaker
 *
 * @param[in] matter_devices_obj The MTDevices delta object; ownership is consumed by this function
 */
void app_rmaker_matter_report_publish_matter_devices_delta(cJSON *matter_devices_obj);

/**
 * @brief Publish a node online-state delta to RainMaker and dispatch the online-state event
 *
 * @param[in] node_id The Matter node ID
 * @param[in] rainmaker_node_id The RainMaker node ID for the Matter node
 * @param[in] online The node online state to publish
 */
void app_rmaker_matter_report_publish_online(uint64_t node_id, const char *rainmaker_node_id, bool online);

/**
 * @brief Check whether a pending attribute delta tree is empty
 *
 * @param[in] pending_root The pending delta root
 *
 * @return true if the tree is NULL or has no pending nodes
 * @return false if the tree contains pending data
 */
bool app_rmaker_matter_report_json_pending_is_empty(cJSON *pending_root);

/**
 * @brief Merge a cJSON attribute value into a pending MTDevices delta
 *
 * @param[in,out] pending_root The pending delta root pointer
 * @param[in] node_id The Matter node ID
 * @param[in] rainmaker_node_id The RainMaker node ID for the Matter node
 * @param[in] endpoint_id The Matter endpoint ID
 * @param[in] cluster_id The Matter cluster ID
 * @param[in] attribute_id The Matter attribute ID
 * @param[in] value The read-only cJSON attribute value
 *
 * @return true if the pending delta was updated
 * @return false if the input was invalid or allocation failed
 */
bool app_rmaker_matter_report_json_merge_pending_attr_item(cJSON **pending_root, uint64_t node_id,
                                                         const char *rainmaker_node_id, uint16_t endpoint_id,
                                                         uint32_t cluster_id, uint32_t attribute_id,
                                                         const cJSON *value);

/**
 * @brief Merge an endpoint tombstone into a pending MTDevices delta
 *
 * @param[in,out] pending_root The pending delta root pointer
 * @param[in] node_id The Matter node ID
 * @param[in] rainmaker_node_id The RainMaker node ID for the Matter node
 * @param[in] endpoint_id The Matter endpoint ID
 * @return true if the endpoint tombstone is present in the pending delta
 * @return false if allocation failed
 */
bool app_rmaker_matter_report_json_merge_pending_endpoint_tombstone(cJSON **pending_root, uint64_t node_id,
                                                                    const char *rainmaker_node_id,
                                                                    uint16_t endpoint_id);

/**
 * @brief Detach and return all pending MTDevices deltas
 *
 * @param[in,out] pending_root The pending delta root pointer
 *
 * @return Detached pending delta object on success
 * @return NULL if no pending delta is available
 */
cJSON *app_rmaker_matter_report_json_detach_pending_all(cJSON **pending_root);

/**
 * @brief Detach and return the pending MTDevices delta for one Matter node
 *
 * @param[in,out] pending_root The pending delta root pointer
 * @param[in] node_id The Matter node ID
 *
 * @return Detached pending node delta object on success
 * @return NULL if no pending delta is available for the node
 */
cJSON *app_rmaker_matter_report_json_detach_pending_node(cJSON **pending_root, uint64_t node_id);

#ifdef __cplusplus
}
#endif
