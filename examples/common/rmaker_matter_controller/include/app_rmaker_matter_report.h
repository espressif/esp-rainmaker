/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cJSON.h>
#include <esp_err.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_RMAKER_MATTER_REPORT_ONLINE = 0,
    APP_RMAKER_MATTER_REPORT_ATTR,
} app_rmaker_matter_report_type_t;

typedef struct {
    app_rmaker_matter_report_type_t type;
    uint64_t node_id;
    /* Possible data shapes:
     * - APP_RMAKER_MATTER_REPORT_ONLINE: {"online": true|false}
     * - APP_RMAKER_MATTER_REPORT_ATTR: {"<node_id>": {"<endpoint_id>": {"<cluster_id>": {"<attribute_id>": {"value": ...}}}}}
     */
    const cJSON *data;
} app_rmaker_matter_report_t;

typedef void (*app_rmaker_matter_report_callback_t)(const app_rmaker_matter_report_t *report, void *priv_data);

/**
 * @brief Register an observer for Matter reports and node online status changes.
 *
 * Reports are invoked from the report task using the same cJSON object being processed by that task. Attribute reports
 * are delivered as coalesced ingress batches. Online reports use data {"online": true|false}. Callback data is
 * read-only and valid only during the callback; duplicate it before posting work elsewhere. Registering NULL
 * unregisters the callback.
 */
esp_err_t app_rmaker_matter_report_set_callback(app_rmaker_matter_report_callback_t cb, void *priv_data);

/**
 * @brief Retry the Matter report subscription for a tracked offline node immediately.
 */
esp_err_t app_rmaker_matter_report_retry_subscription(uint64_t node_id);

/**
 * @brief Retry Matter report subscriptions for all tracked offline nodes immediately.
 */
esp_err_t app_rmaker_matter_report_retry_subscriptions(void);

#ifdef __cplusplus
}
#endif
