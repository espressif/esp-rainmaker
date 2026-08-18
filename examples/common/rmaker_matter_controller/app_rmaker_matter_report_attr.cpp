/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <app_rmaker_matter_report_internal.h>
#include <app_rmaker_matter_report_json.h>
#include <app_rmaker_matter_json_helpers.h>

#include <esp_log.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>

#define TAG "rmaker_matter_report"

static cJSON *get_or_create_object(cJSON *parent, const char *key)
{
    cJSON *obj = cJSON_GetObjectItem(parent, key);
    if (obj) {
        return cJSON_IsObject(obj) ? obj : NULL;
    }
    obj = cJSON_CreateObject();
    if (!obj) {
        return NULL;
    }
    if (!cJSON_AddItemToObject(parent, key, obj)) {
        cJSON_Delete(obj);
        return NULL;
    }
    return obj;
}

static bool parse_unsigned_key(const char *key, int base, uint64_t max_value, uint64_t *value)
{
    if (!key || !*key || !value) {
        return false;
    }
    char *end = NULL;
    unsigned long long parsed = strtoull(key, &end, base);
    if (!end || *end != '\0' || parsed > max_value) {
        return false;
    }
    *value = parsed;
    return true;
}

/* Requires s_state_mutex held. Takes ownership of value. */
static bool merge_ingress_attr_locked(uint64_t node_id, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, cJSON *value)
{
    if (!value) {
        return false;
    }
    if (!s_ingress_attr_delta) {
        s_ingress_attr_delta = cJSON_CreateObject();
        if (!s_ingress_attr_delta) {
            cJSON_Delete(value);
            return false;
        }
    }

    char node_key[32];
    char ep_key[24];
    char cluster_key[24];
    char attr_key[24];
    snprintf(node_key, sizeof(node_key), "%016llx", (unsigned long long)node_id);
    snprintf(ep_key, sizeof(ep_key), "0x%X", (unsigned)endpoint_id);
    snprintf(cluster_key, sizeof(cluster_key), "0x%" PRIX32, cluster_id);
    snprintf(attr_key, sizeof(attr_key), "0x%" PRIX32, attribute_id);

    cJSON *node_obj = get_or_create_object(s_ingress_attr_delta, node_key);
    cJSON *ep_obj = node_obj ? get_or_create_object(node_obj, ep_key) : NULL;
    cJSON *cluster_obj = ep_obj ? get_or_create_object(ep_obj, cluster_key) : NULL;
    cJSON *attr_obj = cluster_obj ? get_or_create_object(cluster_obj, attr_key) : NULL;
    if (!attr_obj) {
        cJSON_Delete(value);
        return false;
    }

    cJSON *old = cJSON_DetachItemFromObject(attr_obj, "value");
    if (!cJSON_AddItemToObject(attr_obj, "value", value)) {
        if (old && !cJSON_AddItemToObject(attr_obj, "value", old)) {
            cJSON_Delete(old);
        }
        cJSON_Delete(value);
        return false;
    }
    cJSON_Delete(old);
    return true;
}

static void ingress_timer_cb(void *arg)
{
    (void)arg;
    if (!s_report_queue || !s_state_mutex) {
        return;
    }
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    cJSON *ingress_batch = app_rmaker_matter_report_json_detach_pending_all(&s_ingress_attr_delta);
    s_ingress_timer_armed = false;
    if (!ingress_batch) {
        xSemaphoreGive(s_state_mutex);
        return;
    }
    report_msg_t msg = {};
    msg.msg_type = REPORT_MSG_ATTR;
    msg.data = ingress_batch;
    if (xQueueSend(s_report_queue, &msg, 0) == pdTRUE) {
        xSemaphoreGive(s_state_mutex);
        return;
    }
    ESP_LOGW(TAG, "Report queue full, retry attr ingress debounce");
    s_ingress_attr_delta = ingress_batch;
    if (s_ingress_timer) {
        (void)esp_timer_start_once(s_ingress_timer, (uint64_t)MATTER_REPORT_DEBOUNCE_MS * 1000ULL);
        s_ingress_timer_armed = true;
    }
    xSemaphoreGive(s_state_mutex);
}

static esp_err_t ensure_ingress_timer_locked(void)
{
    if (s_ingress_timer) {
        return ESP_OK;
    }
    esp_timer_create_args_t args = {
        .callback = &ingress_timer_cb,
        .name = "mt_attr_in",
    };
    return esp_timer_create(&args, &s_ingress_timer);
}

static void arm_ingress_timer_locked(void)
{
    if (app_rmaker_matter_report_json_pending_is_empty(s_ingress_attr_delta)) {
        return;
    }
    if (s_ingress_timer_armed) {
        return;
    }
    if (ensure_ingress_timer_locked() != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create attr ingress timer");
        return;
    }
    if (esp_timer_start_once(s_ingress_timer, (uint64_t)MATTER_REPORT_DEBOUNCE_MS * 1000ULL) == ESP_OK) {
        s_ingress_timer_armed = true;
    } else {
        ESP_LOGW(TAG, "Failed to arm attr ingress timer");
    }
}

void app_rmaker_matter_report_attr_capture(uint64_t node_id, const chip::app::ConcreteDataAttributePath &path,
                                           chip::TLV::TLVReader *data)
{
    if (!s_report_queue || !s_state_mutex) {
        return;
    }

    cJSON *value = NULL;
    if (data) {
        if (app_rmaker_matter_tlv_to_json(*data, &value) != ESP_OK || !value) {
            ESP_LOGW(TAG, "Failed to decode attr TLV for node 0x%llX ep %u", (unsigned long long)node_id,
                     path.mEndpointId);
            cJSON_Delete(value);
            return;
        }
    } else {
        value = cJSON_CreateNull();
        if (!value) {
            return;
        }
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    bool merged = merge_ingress_attr_locked(node_id, path.mEndpointId, path.mClusterId, path.mAttributeId, value);
    if (merged) {
        arm_ingress_timer_locked();
    }
    xSemaphoreGive(s_state_mutex);
}

static bool parts_list_contains(const cJSON *parts_list, uint16_t endpoint_id)
{
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, parts_list)
    {
        if (cJSON_IsNumber(item) && item->valueint == endpoint_id) {
            return true;
        }
    }
    return false;
}

/* Reconcile the node's endpoint cache with endpoint 0 Descriptor.PartsList. */
static void process_parts_list(uint64_t node_id, const cJSON *parts_list)
{
    if (!cJSON_IsArray(parts_list)) {
        return;
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    node_state_t *ns = app_rmaker_matter_report_find_node(node_id);
    if (!ns) {
        xSemaphoreGive(s_state_mutex);
        return;
    }

    bool pending_changed = false;
    bool topology_current = true;
    cJSON *endpoint = ns->root->child;
    while (endpoint) {
        cJSON *next = endpoint->next;
        uint64_t endpoint_id = 0;
        if (parse_unsigned_key(endpoint->string, 0, UINT16_MAX, &endpoint_id) &&
                !parts_list_contains(parts_list, (uint16_t)endpoint_id)) {
            if (app_rmaker_matter_report_json_merge_pending_endpoint_tombstone(
                    &s_pending_attr_delta, ns->node_id, ns->rainmaker_node_id, (uint16_t)endpoint_id)) {
                cJSON_Delete(cJSON_DetachItemViaPointer(ns->root, endpoint));
                pending_changed = true;
            } else {
                topology_current = false;
            }
        }
        endpoint = next;
    }

    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, parts_list)
    {
        if (!cJSON_IsNumber(item)) {
            continue;
        }
        uint16_t endpoint_id = (uint16_t)item->valueint;
        char ep_key[24];
        snprintf(ep_key, sizeof(ep_key), "0x%X", (unsigned)endpoint_id);
        if (!cJSON_GetObjectItem(ns->root, ep_key)) {
            cJSON *endpoint_obj = cJSON_CreateObject();
            if (!endpoint_obj || !cJSON_AddItemToObject(ns->root, ep_key, endpoint_obj)) {
                cJSON_Delete(endpoint_obj);
                topology_current = false;
            }
        }
    }
    ns->parts_list_known = topology_current;
    if (pending_changed) {
        app_rmaker_matter_report_arm_batch_timer_locked();
    }
    xSemaphoreGive(s_state_mutex);
}

static void process_parts_lists(cJSON *batch)
{
    cJSON *node_obj = NULL;
    cJSON_ArrayForEach(node_obj, batch)
    {
        uint64_t node_id = 0;
        if (!cJSON_IsObject(node_obj) || !parse_unsigned_key(node_obj->string, 16, UINT64_MAX, &node_id)) {
            continue;
        }
        cJSON *ep_obj = cJSON_GetObjectItem(node_obj, "0x0");
        cJSON *cluster_obj = ep_obj ? cJSON_GetObjectItem(ep_obj, "0x1D") : NULL;
        cJSON *attr_obj = cluster_obj ? cJSON_GetObjectItem(cluster_obj, "0x3") : NULL;
        cJSON *parts_list = attr_obj ? cJSON_GetObjectItem(attr_obj, "value") : NULL;
        if (parts_list) {
            process_parts_list(node_id, parts_list);
        }
        cJSON_DeleteItemFromObject(node_obj, "0x0");
    }
}

static bool endpoint_is_present(uint64_t node_id, uint16_t endpoint_id)
{
    bool present = true;
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    node_state_t *ns = app_rmaker_matter_report_find_node(node_id);
    if (ns && ns->parts_list_known) {
        char ep_key[24];
        snprintf(ep_key, sizeof(ep_key), "0x%X", (unsigned)endpoint_id);
        present = cJSON_GetObjectItem(ns->root, ep_key) != NULL;
    }
    xSemaphoreGive(s_state_mutex);
    return present;
}

static void process_ingress_attribute(uint64_t node_id, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, const cJSON *value)
{
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    node_state_t *ns = app_rmaker_matter_report_find_node(node_id);
    if (!ns) {
        xSemaphoreGive(s_state_mutex);
        return;
    }
    app_rmaker_matter_report_stop_resubscribe_timer(ns);
    app_rmaker_matter_report_stop_subscribe_timeout(ns);
    app_rmaker_matter_report_reset_fib_backoff(ns);

    if (!app_rmaker_matter_report_json_tree_item_matches(ns->root, endpoint_id, cluster_id, attribute_id, value) &&
            app_rmaker_matter_report_json_merge_pending_attr_item(&s_pending_attr_delta, ns->node_id,
                                                                 ns->rainmaker_node_id, endpoint_id, cluster_id,
                                                                 attribute_id, value)) {
        app_rmaker_matter_report_arm_batch_timer_locked();
        (void)app_rmaker_matter_report_json_update_tree_item(ns->root, endpoint_id, cluster_id, attribute_id, value);
    }
    xSemaphoreGive(s_state_mutex);
}

static void process_ingress_batch(cJSON *batch)
{
    if (!batch) {
        return;
    }
    process_parts_lists(batch);

    cJSON *node_obj = NULL;
    cJSON_ArrayForEach(node_obj, batch)
    {
        uint64_t node_id = 0;
        if (!cJSON_IsObject(node_obj) || !parse_unsigned_key(node_obj->string, 16, UINT64_MAX, &node_id)) {
            continue;
        }
        cJSON *ep_obj = node_obj->child;
        while (ep_obj) {
            cJSON *next_ep_obj = ep_obj->next;
            uint64_t endpoint_id = 0;
            if (!cJSON_IsObject(ep_obj) || !parse_unsigned_key(ep_obj->string, 0, UINT16_MAX, &endpoint_id)) {
                ep_obj = next_ep_obj;
                continue;
            }
            if (!endpoint_is_present(node_id, (uint16_t)endpoint_id)) {
                cJSON_Delete(cJSON_DetachItemViaPointer(node_obj, ep_obj));
                ep_obj = next_ep_obj;
                continue;
            }
            cJSON *cluster_obj = NULL;
            cJSON_ArrayForEach(cluster_obj, ep_obj)
            {
                uint64_t cluster_id = 0;
                if (!cJSON_IsObject(cluster_obj) || !parse_unsigned_key(cluster_obj->string, 0, UINT32_MAX, &cluster_id)) {
                    continue;
                }
                cJSON *attr_obj = NULL;
                cJSON_ArrayForEach(attr_obj, cluster_obj)
                {
                    uint64_t attribute_id = 0;
                    if (!cJSON_IsObject(attr_obj) ||
                            !parse_unsigned_key(attr_obj->string, 0, UINT32_MAX, &attribute_id)) {
                        continue;
                    }
                    cJSON *value = cJSON_GetObjectItem(attr_obj, "value");
                    if (!value) {
                        continue;
                    }
                    process_ingress_attribute(node_id, (uint16_t)endpoint_id, (uint32_t)cluster_id,
                                              (uint32_t)attribute_id, value);
                }
            }
            ep_obj = next_ep_obj;
        }
    }

    node_obj = batch->child;
    while (node_obj) {
        cJSON *next_node_obj = node_obj->next;
        if (cJSON_IsObject(node_obj) && !node_obj->child) {
            cJSON_Delete(cJSON_DetachItemViaPointer(batch, node_obj));
        }
        node_obj = next_node_obj;
    }
    if (batch->child) {
        app_rmaker_matter_report_t report = {
            .type = APP_RMAKER_MATTER_REPORT_ATTR,
            .node_id = 0,
            .data = batch,
        };
        app_rmaker_matter_report_dispatch(&report);
    }
    cJSON_Delete(batch);
}

void app_rmaker_matter_report_attr_process_ingress(cJSON *batch)
{
    process_ingress_batch(batch);
}
