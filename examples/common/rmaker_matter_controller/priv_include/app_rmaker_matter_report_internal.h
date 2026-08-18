/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cJSON.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
#include <app/ConcreteAttributePath.h>
#include <lib/core/TLVReader.h>
#endif

#include <app_rmaker_matter_report.h>
#include <app_rmaker_matter_device_list.h>

#define MATTER_REPORT_DEBOUNCE_MS 250
#define MATTER_REPORT_FIB_MAX_SEC 3600u
#define MATTER_REPORT_FIB_FIRST_SEC 60u
#define MATTER_REPORT_ROOT_ENDPOINT_ID 0
#define MATTER_REPORT_DESCRIPTOR_CLUSTER_ID 0x1D
#define MATTER_REPORT_PARTS_LIST_ATTRIBUTE_ID 0x3

typedef enum {
    REPORT_MSG_ATTR = 0,
    REPORT_MSG_SUBSCRIPTION_TERMINATED,
    REPORT_MSG_RESUBSCRIBE_RETRY,
    REPORT_MSG_SUBSCRIBE_CONNECT_FAILED,
    REPORT_MSG_SUBSCRIPTION_ESTABLISHED,
    REPORT_MSG_SUBSCRIBE_TIMEOUT,
    REPORT_MSG_FLUSH_BATCH,
} report_msg_type_t;

typedef struct {
    report_msg_type_t msg_type;
    uint64_t node_id;
    cJSON *data;
} report_msg_t;

typedef enum {
    REPORT_SUB_STATE_OFFLINE = 0,
    REPORT_SUB_STATE_SUBSCRIBING,
    REPORT_SUB_STATE_ONLINE,
} report_sub_state_t;

typedef struct node_state {
    uint64_t node_id;
    char rainmaker_node_id[ESP_RAINMAKER_NODE_ID_MAX_LEN];
    cJSON *root;
    bool parts_list_known;
    report_sub_state_t sub_state;
    uint32_t fib_prev_sec;
    uint32_t fib_cur_sec;
    esp_timer_handle_t resubscribe_timer;
    esp_timer_handle_t subscribe_timeout_timer;
    struct node_state *next;
} node_state_t;

#ifdef __cplusplus
extern "C" {
#endif

extern QueueHandle_t s_report_queue;
extern SemaphoreHandle_t s_state_mutex;
extern node_state_t *s_node_states;
extern cJSON *s_pending_attr_delta;
extern cJSON *s_ingress_attr_delta;
extern esp_timer_handle_t s_ingress_timer;
extern bool s_ingress_timer_armed;
extern app_rmaker_matter_report_callback_t s_report_cb;
extern void *s_report_cb_priv;

/**
 * @brief Find tracked attribute-report state for a Matter node
 *
 * @param[in] node_id The Matter node ID
 *
 * @return Node state on success
 * @return NULL if the node is not tracked
 */
node_state_t *app_rmaker_matter_report_find_node(uint64_t node_id);

/**
 * @brief Stop a node resubscribe retry timer
 *
 * @param[in,out] ns The node state
 */
void app_rmaker_matter_report_stop_resubscribe_timer(node_state_t *ns);

/**
 * @brief Stop a node subscribe timeout timer
 *
 * @param[in,out] ns The node state
 */
void app_rmaker_matter_report_stop_subscribe_timeout(node_state_t *ns);

/**
 * @brief Start a node subscribe timeout timer
 *
 * @param[in,out] ns The node state
 */
void app_rmaker_matter_report_start_subscribe_timeout(node_state_t *ns);

/**
 * @brief Reset a node Fibonacci resubscribe backoff
 *
 * @param[in,out] ns The node state
 */
void app_rmaker_matter_report_reset_fib_backoff(node_state_t *ns);

/**
 * @brief Schedule a node resubscribe attempt using Fibonacci backoff
 *
 * @param[in,out] ns The node state
 */
void app_rmaker_matter_report_schedule_resubscribe_attempt(node_state_t *ns);

/**
 * @brief Initialize attribute-report publishing runtime
 */
void app_rmaker_matter_report_publish_init(void);

/**
 * @brief Arm the attribute-report batch flush timer
 */
void app_rmaker_matter_report_arm_batch_timer_locked(void);

/**
 * @brief Flush all pending attribute deltas
 *
 * @param[in] force Whether to bypass token-bucket throttling
 *
 * @return Detached pending delta object on success
 * @return NULL if no delta was flushed
 */
cJSON *app_rmaker_matter_report_flush_pending_all_locked(bool force);

/**
 * @brief Flush pending attribute deltas for one Matter node
 *
 * @param[in] node_id The Matter node ID
 * @param[in] force Whether to bypass token-bucket throttling
 *
 * @return Detached pending node delta object on success
 * @return NULL if no delta was flushed
 */
cJSON *app_rmaker_matter_report_flush_pending_node_locked(uint64_t node_id, bool force);

/**
 * @brief Dispatch a report to the registered callback
 *
 * @param[in] report The callback-lifetime report object
 */
void app_rmaker_matter_report_dispatch(const app_rmaker_matter_report_t *report);

/**
 * @brief Handle a subscribe connection failure event
 *
 * @param[in] node_id The Matter node ID, or 0 if the failing node is unknown
 */
void app_rmaker_matter_report_subscribe_handle_connect_failed(uint64_t node_id);

/**
 * @brief Handle a scheduled resubscribe retry event
 *
 * @param[in] node_id The Matter node ID to retry
 */
void app_rmaker_matter_report_subscribe_handle_resubscribe_retry(uint64_t node_id);

/**
 * @brief Handle subscription establishment for a Matter node
 *
 * @param[in] node_id The Matter node ID
 */
void app_rmaker_matter_report_subscribe_handle_established(uint64_t node_id);

/**
 * @brief Handle subscription termination for a Matter node
 *
 * @param[in] node_id The Matter node ID
 */
void app_rmaker_matter_report_subscribe_handle_terminated(uint64_t node_id);

/**
 * @brief Handle subscription timeout for a Matter node
 *
 * @param[in] node_id The Matter node ID
 */
void app_rmaker_matter_report_subscribe_handle_timeout(uint64_t node_id);

/**
 * @brief Send a wildcard Matter attribute subscribe request to a node
 *
 * @param[in] node_id The Matter node ID
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_matter_report_subscribe_send_wildcard(uint64_t node_id);

/**
 * @brief Shut down all Matter subscriptions for a node
 *
 * @param[in] node_id The Matter node ID
 */
void app_rmaker_matter_report_subscribe_shutdown(uint64_t node_id);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

/**
 * @brief Capture one Matter attribute report into the ingress debounce path
 *
 * @param[in] node_id The Matter node ID
 * @param[in] path The Matter concrete attribute path
 * @param[in] data The Matter TLV reader for the attribute value
 */
void app_rmaker_matter_report_attr_capture(uint64_t node_id,
                                           const chip::app::ConcreteDataAttributePath &path,
                                           chip::TLV::TLVReader *data);

/**
 * @brief Process debounced ingress attribute reports
 */
void app_rmaker_matter_report_attr_process_ingress(cJSON *batch);

#endif
