/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <app_rmaker_matter_report_internal.h>
#include <app_rmaker_matter_report_json.h>

#include <esp_log.h>
#include <esp_matter_controller_subscribe_command.h>
#include <esp_matter_controller_utils.h>
#include <esp_matter_core.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <inttypes.h>
#include <string.h>

using namespace esp_matter::controller;

#define TAG "rmaker_matter_report"

/* Filter: retain root PartsList for topology; ignore other endpoint 0 and Descriptor attributes. */
#define MATTER_REPORT_FILTER_GLOBAL_ATTR_MIN 0xFFF8
#define MATTER_REPORT_FILTER_GLOBAL_ATTR_MAX 0xFFFD

static bool should_ignore_attribute(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id)
{
    if (endpoint_id == MATTER_REPORT_ROOT_ENDPOINT_ID && cluster_id == MATTER_REPORT_DESCRIPTOR_CLUSTER_ID &&
            attribute_id == MATTER_REPORT_PARTS_LIST_ATTRIBUTE_ID) {
        return false;
    }
    if (endpoint_id == MATTER_REPORT_ROOT_ENDPOINT_ID) {
        return true;
    }
    if (attribute_id >= MATTER_REPORT_FILTER_GLOBAL_ATTR_MIN && attribute_id <= MATTER_REPORT_FILTER_GLOBAL_ATTR_MAX) {
        return true;
    }
    if (cluster_id == MATTER_REPORT_DESCRIPTOR_CLUSTER_ID) {
        return true;
    }
    return false;
}

/* Requires s_state_mutex held. */
static bool mark_online_locked(node_state_t *ns, char *rainmaker_node_id, size_t rainmaker_node_id_len,
                               cJSON **pending_payload)
{
    if (!ns || !rainmaker_node_id || rainmaker_node_id_len == 0 || !pending_payload) {
        return false;
    }
    ns->sub_state = REPORT_SUB_STATE_ONLINE;
    app_rmaker_matter_report_stop_resubscribe_timer(ns);
    app_rmaker_matter_report_stop_subscribe_timeout(ns);
    app_rmaker_matter_report_reset_fib_backoff(ns);
    strncpy(rainmaker_node_id, ns->rainmaker_node_id, rainmaker_node_id_len - 1);
    rainmaker_node_id[rainmaker_node_id_len - 1] = '\0';
    *pending_payload = app_rmaker_matter_report_flush_pending_node_locked(ns->node_id, true);
    return true;
}

static void on_subscribe_connect_failure_cb(void *context)
{
    (void)context;
    ESP_LOGW(TAG, "Subscribe connect failed");
    report_msg_t refresh = {};
    refresh.msg_type = REPORT_MSG_SUBSCRIBE_CONNECT_FAILED;
    refresh.node_id = 0; /* esp-matter 1.5 callback does not expose node id; retry all offline nodes. */
    if (s_report_queue) {
        xQueueSend(s_report_queue, &refresh, 0);
    }
}

static void on_subscribe_done_cb(uint64_t node_id, uint32_t subscription_id)
{
    (void)subscription_id;
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    node_state_t *ns = app_rmaker_matter_report_find_node(node_id);
    if (ns) {
        app_rmaker_matter_report_stop_subscribe_timeout(ns);
    }
    xSemaphoreGive(s_state_mutex);

    report_msg_t refresh = {};
    refresh.msg_type = REPORT_MSG_SUBSCRIPTION_TERMINATED;
    refresh.node_id = node_id;
    if (s_report_queue) {
        xQueueSend(s_report_queue, &refresh, 0);
    }
}

static void report_online(uint64_t remote_node_id)
{
    cJSON *pending_payload = NULL;
    char online_rainmaker_node_id[ESP_RAINMAKER_NODE_ID_MAX_LEN] = {};
    bool marked_online = false;

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    node_state_t *ns = app_rmaker_matter_report_find_node(remote_node_id);
    if (!ns || ns->sub_state == REPORT_SUB_STATE_ONLINE) {
        xSemaphoreGive(s_state_mutex);
        return;
    }
    if (!s_report_queue) {
        xSemaphoreGive(s_state_mutex);
        return;
    }
    report_msg_t m = {};
    m.msg_type = REPORT_MSG_SUBSCRIPTION_ESTABLISHED;
    m.node_id = remote_node_id;
    if (xQueueSend(s_report_queue, &m, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Attr report queue full, mark subscription-established inline for 0x%llX",
                 remote_node_id);
        marked_online = mark_online_locked(ns, online_rainmaker_node_id, sizeof(online_rainmaker_node_id),
                                           &pending_payload);
    } else {
        ns->sub_state = REPORT_SUB_STATE_ONLINE;
        app_rmaker_matter_report_stop_subscribe_timeout(ns);
    }
    xSemaphoreGive(s_state_mutex);
    app_rmaker_matter_report_publish_matter_devices_delta(pending_payload);
    if (marked_online) {
        app_rmaker_matter_report_publish_online(remote_node_id, online_rainmaker_node_id, true);
    }
}

static void on_attribute_data_cb(uint64_t remote_node_id, const chip::app::ConcreteDataAttributePath &path,
                                 chip::TLV::TLVReader *data)
{
    if (should_ignore_attribute(path.mEndpointId, path.mClusterId, path.mAttributeId)) {
        return;
    }
    report_online(remote_node_id);
    app_rmaker_matter_report_attr_capture(remote_node_id, path, data);
}

esp_err_t app_rmaker_matter_report_subscribe_send_wildcard(uint64_t node_id)
{
    esp_matter::lock::ScopedChipStackLock chip_lock(portMAX_DELAY);
    chip::Platform::ScopedMemoryBufferWithSize<chip::app::AttributePathParams> attr_paths;
    chip::Platform::ScopedMemoryBufferWithSize<chip::app::EventPathParams> event_paths;
    if (!attr_paths.Alloc(1)) {
        return ESP_ERR_NO_MEM;
    }
    attr_paths[0] = chip::app::AttributePathParams(0xFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
    subscribe_command *cmd = chip::Platform::New<subscribe_command>(node_id, std::move(attr_paths), std::move(event_paths),
                                                                    0, 600, false, on_attribute_data_cb,
                                                                    nullptr, on_subscribe_done_cb,
                                                                    on_subscribe_connect_failure_cb);
    if (!cmd) {
        return ESP_ERR_NO_MEM;
    }
    return cmd->send_command();
}

void app_rmaker_matter_report_subscribe_shutdown(uint64_t node_id)
{
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    send_shutdown_subscriptions(node_id);
}

void app_rmaker_matter_report_subscribe_handle_connect_failed(uint64_t node_id)
{
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    if (node_id == 0) {
        for (node_state_t *ns = s_node_states; ns != NULL; ns = ns->next) {
            if (ns->sub_state != REPORT_SUB_STATE_ONLINE) {
                app_rmaker_matter_report_schedule_resubscribe_attempt(ns);
            }
        }
    } else {
        node_state_t *ns = app_rmaker_matter_report_find_node(node_id);
        if (ns && ns->sub_state != REPORT_SUB_STATE_ONLINE) {
            ESP_LOGW(TAG, "Subscribe connect failed for 0x%llX, scheduling backoff retry",
                     (unsigned long long)node_id);
            app_rmaker_matter_report_schedule_resubscribe_attempt(ns);
        }
    }
    xSemaphoreGive(s_state_mutex);
}

void app_rmaker_matter_report_subscribe_handle_resubscribe_retry(uint64_t node_id)
{
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    node_state_t *ns = app_rmaker_matter_report_find_node(node_id);
    if (!ns) {
        xSemaphoreGive(s_state_mutex);
        return;
    }
    if (ns->sub_state == REPORT_SUB_STATE_ONLINE) {
        app_rmaker_matter_report_stop_resubscribe_timer(ns);
        app_rmaker_matter_report_stop_subscribe_timeout(ns);
        xSemaphoreGive(s_state_mutex);
        return;
    }
    if (ns->sub_state == REPORT_SUB_STATE_SUBSCRIBING) {
        xSemaphoreGive(s_state_mutex);
        return;
    }
    uint64_t retry_node_id = ns->node_id;
    ns->sub_state = REPORT_SUB_STATE_SUBSCRIBING;
    app_rmaker_matter_report_start_subscribe_timeout(ns);
    xSemaphoreGive(s_state_mutex);

    esp_err_t err = app_rmaker_matter_report_subscribe_send_wildcard(retry_node_id);
    if (err == ESP_OK) {
        return;
    }

    cJSON *pending_payload = NULL;
    char offline_rainmaker_node_id[ESP_RAINMAKER_NODE_ID_MAX_LEN] = {};
    bool marked_offline = false;

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    ns = app_rmaker_matter_report_find_node(retry_node_id);
    if (ns && ns->sub_state == REPORT_SUB_STATE_SUBSCRIBING) {
        ns->sub_state = REPORT_SUB_STATE_OFFLINE;
        app_rmaker_matter_report_stop_subscribe_timeout(ns);
        app_rmaker_matter_report_schedule_resubscribe_attempt(ns);
        pending_payload = app_rmaker_matter_report_flush_pending_node_locked(ns->node_id, true);
        strncpy(offline_rainmaker_node_id, ns->rainmaker_node_id, sizeof(offline_rainmaker_node_id) - 1);
        marked_offline = true;
        ESP_LOGW(TAG, "Resubscribe send_command failed for 0x%llX: %s",
                 (unsigned long long)retry_node_id, esp_err_to_name(err));
    }
    xSemaphoreGive(s_state_mutex);
    app_rmaker_matter_report_publish_matter_devices_delta(pending_payload);
    if (marked_offline) {
        app_rmaker_matter_report_publish_online(retry_node_id, offline_rainmaker_node_id, false);
    }
}

void app_rmaker_matter_report_subscribe_handle_established(uint64_t node_id)
{
    cJSON *pending_payload = NULL;
    char online_rainmaker_node_id[ESP_RAINMAKER_NODE_ID_MAX_LEN] = {};
    bool marked_online = false;

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    node_state_t *ns = app_rmaker_matter_report_find_node(node_id);
    marked_online = mark_online_locked(ns, online_rainmaker_node_id, sizeof(online_rainmaker_node_id),
                                       &pending_payload);
    xSemaphoreGive(s_state_mutex);
    app_rmaker_matter_report_publish_matter_devices_delta(pending_payload);
    if (marked_online) {
        app_rmaker_matter_report_publish_online(node_id, online_rainmaker_node_id, true);
    }
}

void app_rmaker_matter_report_subscribe_handle_timeout(uint64_t node_id)
{
    cJSON *pending_payload = NULL;
    char offline_rainmaker_node_id[ESP_RAINMAKER_NODE_ID_MAX_LEN] = {};
    bool marked_offline = false;

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    node_state_t *ns = app_rmaker_matter_report_find_node(node_id);
    if (ns && ns->sub_state == REPORT_SUB_STATE_SUBSCRIBING) {
        ESP_LOGW(TAG, "Attr subscription timeout for 0x%llX", (unsigned long long)node_id);
        ns->sub_state = REPORT_SUB_STATE_OFFLINE;
        app_rmaker_matter_report_stop_subscribe_timeout(ns);
        pending_payload = app_rmaker_matter_report_flush_pending_node_locked(ns->node_id, true);
        strncpy(offline_rainmaker_node_id, ns->rainmaker_node_id, sizeof(offline_rainmaker_node_id) - 1);
        marked_offline = true;
        app_rmaker_matter_report_schedule_resubscribe_attempt(ns);
    }
    xSemaphoreGive(s_state_mutex);
    app_rmaker_matter_report_publish_matter_devices_delta(pending_payload);
    if (marked_offline) {
        app_rmaker_matter_report_publish_online(node_id, offline_rainmaker_node_id, false);
    }
}

void app_rmaker_matter_report_subscribe_handle_terminated(uint64_t node_id)
{
    cJSON *pending_payload = NULL;
    char offline_rainmaker_node_id[ESP_RAINMAKER_NODE_ID_MAX_LEN] = {};
    bool marked_offline = false;

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    node_state_t *ns = app_rmaker_matter_report_find_node(node_id);
    if (ns && ns->sub_state != REPORT_SUB_STATE_OFFLINE) {
        ns->sub_state = REPORT_SUB_STATE_OFFLINE;
        app_rmaker_matter_report_stop_subscribe_timeout(ns);
        pending_payload = app_rmaker_matter_report_flush_pending_node_locked(ns->node_id, true);
        ns->parts_list_known = false;
        strncpy(offline_rainmaker_node_id, ns->rainmaker_node_id, sizeof(offline_rainmaker_node_id) - 1);
        marked_offline = true;
        app_rmaker_matter_report_schedule_resubscribe_attempt(ns);
    }
    xSemaphoreGive(s_state_mutex);
    app_rmaker_matter_report_publish_matter_devices_delta(pending_payload);
    if (marked_offline) {
        app_rmaker_matter_report_publish_online(node_id, offline_rainmaker_node_id, false);
    }
}
