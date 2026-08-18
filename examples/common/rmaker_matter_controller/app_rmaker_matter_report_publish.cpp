/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <app_rmaker_matter_report_internal.h>
#include <app_rmaker_matter_report_json.h>

#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_timer.h>
#include <stdint.h>

#define TAG "rmaker_matter_report"

#define MATTER_DEVICES_SCHEMA_REVISION 0

static esp_rmaker_param_t *s_attributes_param = NULL;
static esp_timer_handle_t s_batch_timer = NULL;
static uint32_t s_last_token_update_ms = 0;
static uint8_t s_tokens = CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_CAPACITY;
static bool s_batch_timer_armed = false;
static void refill_batch_tokens_locked(uint32_t now)
{
    uint8_t capacity = CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_CAPACITY;
    if (s_tokens > capacity) {
        s_tokens = capacity;
    }
    if (s_last_token_update_ms == 0) {
        s_last_token_update_ms = now;
        s_tokens = capacity;
        return;
    }
    uint32_t refill_ms = CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_REFILL_MS;
    uint32_t elapsed_ms = now - s_last_token_update_ms;
    if (elapsed_ms < refill_ms) {
        return;
    }
    uint32_t refill_count = elapsed_ms / refill_ms;
    if (refill_count > 0) {
        s_tokens = (refill_count >= capacity || s_tokens + refill_count >= capacity) ? capacity : s_tokens + refill_count;
        s_last_token_update_ms += refill_count * refill_ms;
    }
}

static void batch_timer_cb(void *arg)
{
    (void)arg;
    report_msg_t msg = {};
    msg.msg_type = REPORT_MSG_FLUSH_BATCH;
    if (s_report_queue && xQueueSend(s_report_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Attr report queue full, retry batch flush");
        if (!s_state_mutex) {
            return;
        }
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        s_batch_timer_armed = false;
        if (!app_rmaker_matter_report_json_pending_is_empty(s_pending_attr_delta)) {
            app_rmaker_matter_report_arm_batch_timer_locked();
        }
        xSemaphoreGive(s_state_mutex);
    }
}

static esp_err_t ensure_batch_timer_locked(void)
{
    if (s_batch_timer) {
        return ESP_OK;
    }
    esp_timer_create_args_t args = {
        .callback = &batch_timer_cb,
        .name = "mt_attr_batch",
    };
    return esp_timer_create(&args, &s_batch_timer);
}

static void stop_batch_timer_locked(void)
{
    if (s_batch_timer) {
        esp_timer_stop(s_batch_timer);
    }
    s_batch_timer_armed = false;
}

void app_rmaker_matter_report_publish_init(void)
{
    s_last_token_update_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    s_tokens = CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_CAPACITY;
}

void app_rmaker_matter_report_arm_batch_timer_locked(void)
{
    if (app_rmaker_matter_report_json_pending_is_empty(s_pending_attr_delta)) {
        stop_batch_timer_locked();
        return;
    }
    if (s_batch_timer_armed) {
        return;
    }
    if (ensure_batch_timer_locked() != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create attr batch timer");
        return;
    }

    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    refill_batch_tokens_locked(now);
    uint32_t delay_ms = MATTER_REPORT_DEBOUNCE_MS;
    if (s_tokens == 0) {
        uint32_t elapsed_ms = now - s_last_token_update_ms;
        delay_ms = CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_REFILL_MS - elapsed_ms;
        if (delay_ms == 0) {
            delay_ms = 1;
        }
    }
    if (esp_timer_start_once(s_batch_timer, (uint64_t)delay_ms * 1000ULL) == ESP_OK) {
        s_batch_timer_armed = true;
    } else {
        ESP_LOGW(TAG, "Failed to arm attr batch timer");
    }
}

cJSON *app_rmaker_matter_report_flush_pending_all_locked(bool force)
{
    s_batch_timer_armed = false;
    if (app_rmaker_matter_report_json_pending_is_empty(s_pending_attr_delta)) {
        return NULL;
    }
    refill_batch_tokens_locked((uint32_t)(esp_timer_get_time() / 1000ULL));
    if (!force && s_tokens == 0) {
        app_rmaker_matter_report_arm_batch_timer_locked();
        return NULL;
    }
    if (s_tokens > 0) {
        s_tokens--;
    }
    cJSON *payload = app_rmaker_matter_report_json_detach_pending_all(&s_pending_attr_delta);
    stop_batch_timer_locked();
    return payload;
}

cJSON *app_rmaker_matter_report_flush_pending_node_locked(uint64_t node_id, bool force)
{
    if (app_rmaker_matter_report_json_pending_is_empty(s_pending_attr_delta)) {
        return NULL;
    }
    refill_batch_tokens_locked((uint32_t)(esp_timer_get_time() / 1000ULL));
    if (s_tokens == 0 && !force) {
        app_rmaker_matter_report_arm_batch_timer_locked();
        return NULL;
    }
    cJSON *payload = app_rmaker_matter_report_json_detach_pending_node(&s_pending_attr_delta, node_id);
    if (payload && s_tokens > 0) {
        s_tokens--;
    }
    if (app_rmaker_matter_report_json_pending_is_empty(s_pending_attr_delta)) {
        stop_batch_timer_locked();
    }
    return payload;
}

void app_rmaker_matter_report_set_param(esp_rmaker_param_t *matter_devices_param)
{
    s_attributes_param = matter_devices_param;
}

void app_rmaker_matter_report_publish_matter_devices_delta(cJSON *matter_devices_obj)
{
    if (!s_attributes_param) {
        cJSON_Delete(matter_devices_obj);
        return;
    }
    if (!matter_devices_obj) {
        return;
    }
    if (!matter_devices_obj->child) {
        cJSON_Delete(matter_devices_obj);
        return;
    }
    char *payload = cJSON_PrintUnformatted(matter_devices_obj);
    cJSON_Delete(matter_devices_obj);
    if (!payload) {
        return;
    }

    esp_err_t err = esp_rmaker_param_update_and_report(s_attributes_param, esp_rmaker_obj(payload));
    cJSON_free(payload);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to report matter attributes param: %s", esp_err_to_name(err));
    }
}

void app_rmaker_matter_report_publish_online(uint64_t node_id, const char *rainmaker_node_id, bool online)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *wrapper = cJSON_CreateObject();
    if (!root || !wrapper) {
        cJSON_Delete(root);
        cJSON_Delete(wrapper);
    } else {
        char node_key[32];
        snprintf(node_key, sizeof(node_key), "%016llx", (unsigned long long)node_id);
        cJSON_AddItemToObject(wrapper, "rainmaker_node_id", cJSON_CreateString(rainmaker_node_id ? rainmaker_node_id : ""));
        cJSON_AddItemToObject(wrapper, "online", cJSON_CreateBool(online));
        cJSON_AddItemToObject(root, node_key, wrapper);
        app_rmaker_matter_report_publish_matter_devices_delta(root);
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return;
    }
    cJSON_AddBoolToObject(data, "online", online);
    app_rmaker_matter_report_t report = {
        .type = APP_RMAKER_MATTER_REPORT_ONLINE,
        .node_id = node_id,
        .data = data,
    };
    app_rmaker_matter_report_dispatch(&report);
    cJSON_Delete(data);
}
