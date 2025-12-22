/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/** Enable Groups Service
 *
 * This API enables the groups service for the node. This allows phone apps to
 * set the parent group ID (pgrp_id) in which the node is added.
 *
 * Once set, all local params will be reported on
 * node/<node_id>/params/local/<pgrp_id> instead of node/<node_id>/params/local/
 *
 * Additionally, application code can use the esp_rmaker_publish_direct()
 * API to publish any message on node/<node_id>/direct/params/local/<pgrp_id>
 *
 * Phone apps can subscribe to these topics to get these messages directly
 * from the MQTT broker:
 * - node/+/params/local/<pgrp_id>
 * - node/+/direct/params/local/<pgrp_id>
 *
 * For local params update, this change helps to improve the latency
 * of param reporting.
 *
 * The direct reporting allows to bypass the cloud side processing to send
 * messages directly to phone apps at very low cost.
 *
 * Phone apps can also publish param updates directly to
 * node/<node_id>/params/remote/<pgrp_id> instead of using the set params API.
 *
 * @note This API should be called after esp_rmaker_node_init() but before esp_rmaker_start().
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_groups_service_enable(void);

#ifdef __cplusplus
}
#endif
