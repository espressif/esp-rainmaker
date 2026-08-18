/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cJSON.h>
#include <esp_check.h>
#include <esp_err.h>
#if CONFIG_RMAKER_MTCTL_MEMORY_ALLOCATION_PREFER_SPIRAM || CONFIG_RMAKER_MTCTL_MEMORY_REPORT_TASK_CREATE_FROM_SPIRAM
#include <esp_heap_caps.h>
#endif
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#if CONFIG_RMAKER_MTCTL_MEMORY_ALLOCATION_PREFER_SPIRAM
#include <freertos/idf_additions.h>
#endif
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <algorithm>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <lib/core/CHIPConfig.h>

#include <app_rmaker_matter_controller.h>
#include <app_rmaker_matter_controller_internal.h>
#include <app_rmaker_matter_device_list.h>
#include <app_rmaker_matter_report_json.h>
#include <app_rmaker_matter_report_internal.h>

#define TAG "rmaker_matter_report"

#define MATTER_REPORT_TASK_PRIO                 5
#define MATTER_REPORT_INITIAL_SUBSCRIBE_PACE_MS 2000u

constexpr size_t kMaxTrackedNodes = std::min({
    static_cast<size_t>(CONFIG_RMAKER_MTCTL_MAX_DEVICE_COUNT),
    static_cast<size_t>(CHIP_CONFIG_CONTROLLER_MAX_ACTIVE_DEVICES),
    static_cast<size_t>(CHIP_CONFIG_MAX_EXCHANGE_CONTEXTS),
    static_cast<size_t>(CHIP_CONFIG_SECURE_SESSION_POOL_SIZE),
});

static_assert(CONFIG_RMAKER_MTCTL_REPORT_QUEUE_SIZE > 0,
              "CONFIG_RMAKER_MTCTL_REPORT_QUEUE_SIZE must be greater than 0");
static_assert(CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_CAPACITY > 0,
              "CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_CAPACITY must be greater than 0");
static_assert(CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_REFILL_MS > 0,
              "CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_REFILL_MS must be greater than 0");
static_assert(CONFIG_RMAKER_MTCTL_REPORT_SUBSCRIBE_TIMEOUT_MS > 0,
              "CONFIG_RMAKER_MTCTL_REPORT_SUBSCRIBE_TIMEOUT_MS must be greater than 0");
static_assert(kMaxTrackedNodes > 0, "Matter controller must track at least one node");

QueueHandle_t s_report_queue = NULL;
static TaskHandle_t s_report_task = NULL;
#if CONFIG_RMAKER_MTCTL_MEMORY_REPORT_TASK_CREATE_FROM_SPIRAM
static StackType_t *s_report_task_stack = NULL;
static StaticTask_t s_report_task_tcb;
#endif
SemaphoreHandle_t s_state_mutex = NULL;
node_state_t *s_node_states = NULL;
static bool s_report_initialized = false;
cJSON *s_pending_attr_delta = NULL;
cJSON *s_ingress_attr_delta = NULL;
esp_timer_handle_t s_ingress_timer = NULL;
bool s_ingress_timer_armed = false;
app_rmaker_matter_report_callback_t s_report_cb = NULL;
void *s_report_cb_priv = NULL;

node_state_t *app_rmaker_matter_report_find_node(uint64_t node_id)
{
    for (node_state_t *n = s_node_states; n != NULL; n = n->next) {
        if (n->node_id == node_id) {
            return n;
        }
    }
    return NULL;
}

static void free_node_state(node_state_t *ns)
{
    if (ns->resubscribe_timer) {
        esp_timer_stop(ns->resubscribe_timer);
        esp_timer_delete(ns->resubscribe_timer);
        ns->resubscribe_timer = NULL;
    }
    if (ns->subscribe_timeout_timer) {
        esp_timer_stop(ns->subscribe_timeout_timer);
        esp_timer_delete(ns->subscribe_timeout_timer);
        ns->subscribe_timeout_timer = NULL;
    }
    if (ns->root) {
        cJSON_Delete(ns->root);
        ns->root = NULL;
    }
    free(ns);
}

static node_state_t *create_node_state(uint64_t node_id, const char *rainmaker_node_id)
{
#if CONFIG_RMAKER_MTCTL_MEMORY_ALLOCATION_PREFER_SPIRAM
    node_state_t *ns = (node_state_t *)heap_caps_calloc_prefer(1, sizeof(node_state_t), 2,
                                                               MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM,
                                                               MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
#else
    node_state_t *ns = (node_state_t *)calloc(1, sizeof(node_state_t));
#endif
    if (!ns) {
        return NULL;
    }
    ns->node_id = node_id;
    if (rainmaker_node_id) {
        strncpy(ns->rainmaker_node_id, rainmaker_node_id, sizeof(ns->rainmaker_node_id) - 1);
        ns->rainmaker_node_id[sizeof(ns->rainmaker_node_id) - 1] = '\0';
    }
    ns->root = cJSON_CreateObject();
    if (!ns->root) {
        free(ns);
        return NULL;
    }
    ns->sub_state = REPORT_SUB_STATE_OFFLINE;
    ns->fib_prev_sec = 0;
    ns->fib_cur_sec = MATTER_REPORT_FIB_FIRST_SEC;
    ns->resubscribe_timer = NULL;
    ns->subscribe_timeout_timer = NULL;
    return ns;
}

void app_rmaker_matter_report_reset_fib_backoff(node_state_t *ns)
{
    ns->fib_prev_sec = 0;
    ns->fib_cur_sec = MATTER_REPORT_FIB_FIRST_SEC;
}

void app_rmaker_matter_report_stop_resubscribe_timer(node_state_t *ns)
{
    if (ns && ns->resubscribe_timer) {
        esp_timer_stop(ns->resubscribe_timer);
    }
}

void app_rmaker_matter_report_stop_subscribe_timeout(node_state_t *ns)
{
    if (ns && ns->subscribe_timeout_timer) {
        esp_timer_stop(ns->subscribe_timeout_timer);
    }
}

static void resubscribe_timer_cb(void *arg)
{
    node_state_t *ns = (node_state_t *)arg;
    if (!ns || !s_report_queue) {
        return;
    }
    report_msg_t refresh = {};
    refresh.msg_type = REPORT_MSG_RESUBSCRIBE_RETRY;
    refresh.node_id = ns->node_id;
    if (xQueueSend(s_report_queue, &refresh, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Attr report queue full, drop resubscribe retry for 0x%llX",
                 (unsigned long long)ns->node_id);
    }
}

static void subscribe_timeout_timer_cb(void *arg)
{
    node_state_t *ns = (node_state_t *)arg;
    if (!ns || !s_report_queue) {
        return;
    }
    report_msg_t msg = {};
    msg.msg_type = REPORT_MSG_SUBSCRIBE_TIMEOUT;
    msg.node_id = ns->node_id;
    if (xQueueSend(s_report_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Attr report queue full, drop subscribe timeout for 0x%llX",
                 (unsigned long long)ns->node_id);
    }
}

static esp_err_t ensure_resubscribe_timer(node_state_t *ns)
{
    if (ns->resubscribe_timer) {
        return ESP_OK;
    }
    esp_timer_create_args_t args = {
        .callback = &resubscribe_timer_cb,
        .arg = ns,
        .name = "mt_attr_rs",
    };
    return esp_timer_create(&args, &ns->resubscribe_timer);
}

static esp_err_t ensure_subscribe_timeout_timer(node_state_t *ns)
{
    if (ns->subscribe_timeout_timer) {
        return ESP_OK;
    }
    esp_timer_create_args_t args = {
        .callback = &subscribe_timeout_timer_cb,
        .arg = ns,
        .name = "mt_attr_st",
    };
    return esp_timer_create(&args, &ns->subscribe_timeout_timer);
}

void app_rmaker_matter_report_start_subscribe_timeout(node_state_t *ns)
{
    if (!ns) {
        return;
    }
    if (ensure_subscribe_timeout_timer(ns) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create subscribe timeout for 0x%llX", (unsigned long long)ns->node_id);
        return;
    }
    esp_timer_stop(ns->subscribe_timeout_timer);
    esp_err_t err = esp_timer_start_once(ns->subscribe_timeout_timer,
                                         (uint64_t)CONFIG_RMAKER_MTCTL_REPORT_SUBSCRIBE_TIMEOUT_MS * 1000ULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start subscribe timeout for 0x%llX: %s", (unsigned long long)ns->node_id,
                 esp_err_to_name(err));
    }
}

/* Requires s_state_mutex held. Schedules next one-shot retry; advances Fibonacci state. */
void app_rmaker_matter_report_schedule_resubscribe_attempt(node_state_t *ns)
{
    if (!ns) {
        return;
    }
    if (ensure_resubscribe_timer(ns) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create resubscribe timer for 0x%llX", (unsigned long long)ns->node_id);
        return;
    }
    if (esp_timer_is_active(ns->resubscribe_timer)) {
        return;
    }
    uint32_t delay_sec = ns->fib_cur_sec;
    if (delay_sec > MATTER_REPORT_FIB_MAX_SEC) {
        delay_sec = MATTER_REPORT_FIB_MAX_SEC;
    }
    ESP_LOGI(TAG, "Scheduling resubscribe attempt for 0x%llX in %u sec", (unsigned long long)ns->node_id,
             (unsigned)delay_sec);
    esp_err_t err = esp_timer_start_once(ns->resubscribe_timer, (uint64_t)delay_sec * 1000000ULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_timer_start_once failed for 0x%llX: %s", (unsigned long long)ns->node_id,
                 esp_err_to_name(err));
        return;
    }
    uint32_t nxt = ns->fib_prev_sec + ns->fib_cur_sec;
    if (nxt < ns->fib_cur_sec || nxt > MATTER_REPORT_FIB_MAX_SEC) {
        nxt = MATTER_REPORT_FIB_MAX_SEC;
    }
    ns->fib_prev_sec = ns->fib_cur_sec;
    if (ns->fib_prev_sec > MATTER_REPORT_FIB_MAX_SEC) {
        ns->fib_prev_sec = MATTER_REPORT_FIB_MAX_SEC;
    }
    ns->fib_cur_sec = nxt;
}

void app_rmaker_matter_report_dispatch(const app_rmaker_matter_report_t *report)
{
    app_rmaker_matter_report_callback_t cb = NULL;
    void *priv_data = NULL;

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    cb = s_report_cb;
    priv_data = s_report_cb_priv;
    xSemaphoreGive(s_state_mutex);

    if (cb && report) {
        cb(report, priv_data);
    }
}

static void report_task(void *arg)
{
    report_msg_t msg;
    while (xQueueReceive(s_report_queue, &msg, portMAX_DELAY) == pdTRUE) {
        switch (msg.msg_type) {
        case REPORT_MSG_ATTR:
            app_rmaker_matter_report_attr_process_ingress(msg.data);
            msg.data = NULL;
            break;
        case REPORT_MSG_FLUSH_BATCH: {
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            cJSON *payload = app_rmaker_matter_report_flush_pending_all_locked(false);
            xSemaphoreGive(s_state_mutex);
            app_rmaker_matter_report_publish_matter_devices_delta(payload);
            break;
        }
        case REPORT_MSG_SUBSCRIBE_CONNECT_FAILED:
            app_rmaker_matter_report_subscribe_handle_connect_failed(msg.node_id);
            break;
        case REPORT_MSG_RESUBSCRIBE_RETRY:
            app_rmaker_matter_report_subscribe_handle_resubscribe_retry(msg.node_id);
            break;
        case REPORT_MSG_SUBSCRIPTION_ESTABLISHED:
            app_rmaker_matter_report_subscribe_handle_established(msg.node_id);
            break;
        case REPORT_MSG_SUBSCRIBE_TIMEOUT:
            app_rmaker_matter_report_subscribe_handle_timeout(msg.node_id);
            break;
        case REPORT_MSG_SUBSCRIPTION_TERMINATED:
            app_rmaker_matter_report_subscribe_handle_terminated(msg.node_id);
            break;
        default:
            cJSON_Delete(msg.data);
            ESP_LOGW(TAG, "Unknown attr report msg_type %d", (int)msg.msg_type);
            break;
        }
    }
}

esp_err_t app_rmaker_matter_report_enable(void)
{
    if (s_report_queue != NULL) {
        s_report_initialized = true;
        return ESP_OK;
    }
    s_state_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_state_mutex, ESP_ERR_NO_MEM, TAG, "Failed to create state mutex");
#if CONFIG_RMAKER_MTCTL_MEMORY_ALLOCATION_PREFER_SPIRAM
    s_report_queue = xQueueCreateWithCaps(CONFIG_RMAKER_MTCTL_REPORT_QUEUE_SIZE, sizeof(report_msg_t),
                                          MALLOC_CAP_SPIRAM);
#else
    s_report_queue = xQueueCreate(CONFIG_RMAKER_MTCTL_REPORT_QUEUE_SIZE, sizeof(report_msg_t));
#endif
    if (!s_report_queue) {
        vSemaphoreDelete(s_state_mutex);
        s_state_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }
    bool task_created = false;
#if CONFIG_RMAKER_MTCTL_MEMORY_REPORT_TASK_CREATE_FROM_SPIRAM
    s_report_task_stack = (StackType_t *)heap_caps_calloc(CONFIG_RMAKER_MTCTL_REPORT_TASK_STACK_SIZE,
                                                          sizeof(StackType_t),
                                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_report_task_stack) {
        s_report_task = xTaskCreateStatic(report_task, "matter_attr_rpt",
                                          CONFIG_RMAKER_MTCTL_REPORT_TASK_STACK_SIZE, NULL,
                                          MATTER_REPORT_TASK_PRIO, s_report_task_stack,
                                          &s_report_task_tcb);
        task_created = s_report_task != NULL;
    }
#else
    task_created = xTaskCreate(report_task, "matter_attr_rpt",
                               CONFIG_RMAKER_MTCTL_REPORT_TASK_STACK_SIZE, NULL,
                               MATTER_REPORT_TASK_PRIO, &s_report_task) == pdPASS;
#endif
    if (!task_created) {
#if CONFIG_RMAKER_MTCTL_MEMORY_REPORT_TASK_CREATE_FROM_SPIRAM
        free(s_report_task_stack);
        s_report_task_stack = NULL;
#endif
#if CONFIG_RMAKER_MTCTL_MEMORY_ALLOCATION_PREFER_SPIRAM
        vQueueDeleteWithCaps(s_report_queue);
#else
        vQueueDelete(s_report_queue);
#endif
        s_report_queue = NULL;
        vSemaphoreDelete(s_state_mutex);
        s_state_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }
    app_rmaker_matter_report_publish_init();
    s_report_initialized = true;
    return ESP_OK;
}

esp_err_t app_rmaker_matter_report_set_callback(app_rmaker_matter_report_callback_t cb, void *priv_data)
{
    if (s_state_mutex) {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    }
    s_report_cb = cb;
    s_report_cb_priv = cb ? priv_data : NULL;
    if (s_state_mutex) {
        xSemaphoreGive(s_state_mutex);
    }
    return ESP_OK;
}

esp_err_t app_rmaker_matter_report_retry_subscription(uint64_t node_id)
{
    if (!s_report_queue || !s_state_mutex || node_id == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    node_state_t *ns = app_rmaker_matter_report_find_node(node_id);
    if (!ns) {
        xSemaphoreGive(s_state_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    if (ns->sub_state == REPORT_SUB_STATE_ONLINE || ns->sub_state == REPORT_SUB_STATE_SUBSCRIBING) {
        xSemaphoreGive(s_state_mutex);
        return ESP_OK;
    }
    app_rmaker_matter_report_stop_resubscribe_timer(ns);
    xSemaphoreGive(s_state_mutex);

    report_msg_t retry = {};
    retry.msg_type = REPORT_MSG_RESUBSCRIBE_RETRY;
    retry.node_id = node_id;
    return xQueueSend(s_report_queue, &retry, 0) == pdTRUE ? ESP_OK : ESP_FAIL;
}

esp_err_t app_rmaker_matter_report_retry_subscriptions(void)
{
    if (!s_report_queue || !s_state_mutex) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    for (node_state_t *ns = s_node_states; ns != NULL; ns = ns->next) {
        if (ns->sub_state == REPORT_SUB_STATE_ONLINE || ns->sub_state == REPORT_SUB_STATE_SUBSCRIBING) {
            continue;
        }
        app_rmaker_matter_report_stop_resubscribe_timer(ns);
        report_msg_t retry = {};
        retry.msg_type = REPORT_MSG_RESUBSCRIBE_RETRY;
        retry.node_id = ns->node_id;
        if (xQueueSend(s_report_queue, &retry, 0) != pdTRUE) {
            ret = ESP_FAIL;
        }
    }
    xSemaphoreGive(s_state_mutex);
    return ret;
}

esp_err_t app_rmaker_matter_report_on_device_list_update(const matter_device_t *dev_list)
{
    esp_err_t ret = app_rmaker_matter_report_enable();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Ignoring device-list update: failed to initialize attr-report runtime");
        return ret;
    }
    typedef struct {
        uint64_t removed_node_ids[kMaxTrackedNodes];
        cJSON *removed_pending[kMaxTrackedNodes];
        uint64_t new_node_ids[kMaxTrackedNodes];
    } update_work_t;
#if CONFIG_RMAKER_MTCTL_MEMORY_ALLOCATION_PREFER_SPIRAM
    update_work_t *work = (update_work_t *)heap_caps_calloc_prefer(1, sizeof(*work), 2,
                                                                   MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM,
                                                                   MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
#else
    update_work_t *work = (update_work_t *)calloc(1, sizeof(*work));
#endif
    if (!work) {
        ESP_LOGW(TAG, "Ignoring device-list update: failed to allocate update work");
        return ESP_ERR_NO_MEM;
    }
    size_t removed_count = 0;
    size_t new_node_count = 0;
    cJSON *removal = NULL;
    ret = ESP_OK;

    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);

        /* Remove states for nodes no longer in the list (CHIP shutdown done after mutex is released). */
        node_state_t *n = s_node_states;
        node_state_t *prev = NULL;
        while (n != NULL) {
            node_state_t *next = n->next;
            bool node_still_present = false;
            for (const matter_device_t *d = dev_list; d != NULL; d = d->next) {
                if (d->node_id == n->node_id) {
                    node_still_present = true;
                    break;
                }
            }
            if (!node_still_present) {
                uint64_t node_id = n->node_id;
                if (prev) {
                    prev->next = next;
                } else {
                    s_node_states = next;
                }
                work->removed_pending[removed_count] = app_rmaker_matter_report_flush_pending_node_locked(node_id,
                                                                                                          true);
                cJSON_Delete(app_rmaker_matter_report_json_detach_pending_node(&s_ingress_attr_delta, node_id));
                work->removed_node_ids[removed_count++] = node_id;
                free_node_state(n);
                n = next;
                continue;
            }
            prev = n;
            n = next;
        }

        /* Subscribe to nodes in the list that we don't have yet */
        size_t subscribed_count = 0;
        for (const matter_device_t *d = dev_list; d != NULL; d = d->next) {
            if (app_rmaker_matter_report_find_node(d->node_id)) {
                subscribed_count++;
                continue;
            }
            if (subscribed_count < kMaxTrackedNodes) {
                node_state_t *ns = create_node_state(d->node_id, d->rainmaker_node_id);
                if (ns) {
                    ns->next = s_node_states;
                    s_node_states = ns;
                    work->new_node_ids[new_node_count++] = d->node_id;
                    subscribed_count++;
                } else {
                    ESP_LOGW(TAG, "Failed to track attr subscription for 0x%llX: %s",
                             (unsigned long long)d->node_id, esp_err_to_name(ESP_ERR_NO_MEM));
                    ret = ESP_ERR_NO_MEM;
                }
            } else {
                ESP_LOGW(TAG, "Skip attr subscription for 0x%llX: node subscription limit %u reached",
                         (unsigned long long)d->node_id, (unsigned)kMaxTrackedNodes);
            }
        }

        if (removed_count > 0) {
            removal = cJSON_CreateObject();
            if (removal) {
                for (size_t i = 0; i < removed_count; i++) {
                    char node_key[32];
                    snprintf(node_key, sizeof(node_key), "%016llx", (unsigned long long)work->removed_node_ids[i]);
                    cJSON_AddItemToObject(removal, node_key, cJSON_CreateNull());
                }
            }
        }

        xSemaphoreGive(s_state_mutex);
    }

    for (size_t i = 0; i < new_node_count; i++) {
        if (i > 0) {
            vTaskDelay(pdMS_TO_TICKS(MATTER_REPORT_INITIAL_SUBSCRIBE_PACE_MS));
        }
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        node_state_t *ns = app_rmaker_matter_report_find_node(work->new_node_ids[i]);
        if (ns) {
            if (ns->sub_state != REPORT_SUB_STATE_OFFLINE) {
                xSemaphoreGive(s_state_mutex);
                continue;
            }
            ns->sub_state = REPORT_SUB_STATE_SUBSCRIBING;
            app_rmaker_matter_report_start_subscribe_timeout(ns);
        }
        xSemaphoreGive(s_state_mutex);
        esp_err_t err = app_rmaker_matter_report_subscribe_send_wildcard(work->new_node_ids[i]);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to start attr subscription for 0x%llX: %s",
                     (unsigned long long)work->new_node_ids[i], esp_err_to_name(err));
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            ns = app_rmaker_matter_report_find_node(work->new_node_ids[i]);
            if (ns) {
                app_rmaker_matter_report_stop_subscribe_timeout(ns);
            }
            xSemaphoreGive(s_state_mutex);
            app_rmaker_matter_report_subscribe_handle_timeout(work->new_node_ids[i]);
        }
    }

    for (size_t i = 0; i < removed_count; i++) {
        app_rmaker_matter_report_publish_matter_devices_delta(work->removed_pending[i]);
    }
    app_rmaker_matter_report_publish_matter_devices_delta(removal);

    for (size_t i = 0; i < removed_count; i++) {
        app_rmaker_matter_report_subscribe_shutdown(work->removed_node_ids[i]);
    }
    free(work);
    return ret;
}
