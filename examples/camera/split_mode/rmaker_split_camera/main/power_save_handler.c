/*
* SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#include <stdio.h>
#include <esp_err.h>
#include <esp_console.h>
#include <esp_log.h>
#include "app_webrtc.h"

#define SLAVE_HOOKS_FOR_HOST_DEEP_SLEEP (1)
#ifdef CONFIG_ESP_HOSTED_CP_LIGHT_SLEEP_ENABLE
#define SLAVE_LIGHT_SLEEP_ON_HOST_DEEP_SLEEP (1)
#else
#define SLAVE_LIGHT_SLEEP_ON_HOST_DEEP_SLEEP (0)
#endif

#define SUSPEND_NON_CRITICAL_TASKS_WHILE_LIGHT_SLEEPING (0)

#if SUSPEND_NON_CRITICAL_TASKS_WHILE_LIGHT_SLEEPING && !SLAVE_LIGHT_SLEEP_ON_HOST_DEEP_SLEEP
#error "Incompatible config"
#endif

#if SUSPEND_NON_CRITICAL_TASKS_WHILE_LIGHT_SLEEPING
#ifndef CONFIG_FREERTOS_USE_TRACE_FACILITY
    #error "CONFIG_FREERTOS_USE_TRACE_FACILITY is mandatory for SUSPEND_NON_CRITICAL_TASKS_WHILE_LIGHT_SLEEPING to work"
#endif
#endif

static const char *TAG = "power_save";

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_pm.h"
#include "esp_event.h"

/* The Wi-Fi power-save helpers apply on the co-processor targets alone. On any
 * other target they would be dead code. */
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C5)
#define PS_WIFI_KNOBS_ENABLED 1
#else
#define PS_WIFI_KNOBS_ENABLED 0
#endif

#if PS_WIFI_KNOBS_ENABLED

static esp_err_t configure_wifi_power_save(void)
{
#if CONFIG_EXAMPLE_WIFI_PS_NONE
    const wifi_ps_type_t mode = WIFI_PS_NONE;
    const char *mode_name = "NONE";
#elif CONFIG_EXAMPLE_WIFI_PS_MAX_MODEM
    const wifi_ps_type_t mode = WIFI_PS_MAX_MODEM;
    const char *mode_name = "MAX_MODEM";
#else
    const wifi_ps_type_t mode = WIFI_PS_MIN_MODEM;
    const char *mode_name = "MIN_MODEM";
#endif

    /* Read back rather than trust the write. Until this function was wired to
     * an event it never ran, and the radio kept the driver default silently.
     * Any power figure is only meaningful beside this line. */
    wifi_ps_type_t before = WIFI_PS_NONE;
    esp_wifi_get_ps(&before);

    esp_err_t ret = esp_wifi_set_ps(mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_ps(%s) failed: %s", mode_name, esp_err_to_name(ret));
        return ret;
    }

    wifi_ps_type_t after = WIFI_PS_NONE;
    esp_wifi_get_ps(&after);
    ESP_LOGI(TAG, "Wi-Fi power save mode: %s (was %d, now %d)",
             mode_name, (int) before, (int) after);

#if CONFIG_EXAMPLE_WIFI_PS_MAX_MODEM
    /* Only MAX_MODEM honours listen_interval. Under MIN_MODEM the radio wakes
     * at every DTIM, so the field is inert.
     *
     * `wifi-set` rebuilds wifi_config_t from zero and restarts the station, so
     * a one-shot write at boot is lost at the first provisioning. Rewriting it
     * on every WIFI_EVENT_STA_START keeps the value in the driver. The driver
     * reads the field when the station associates, and esp_wifi_start() only
     * posts the event, so a write may land after the association it was meant
     * for. The value then applies from the next association. */
    wifi_config_t wifi_config = {};
    ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
    if (ret == ESP_OK) {
        wifi_config.sta.listen_interval = CONFIG_EXAMPLE_WIFI_LISTEN_INTERVAL;
        ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "listen_interval not applied: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Wi-Fi listen interval: %d beacons", CONFIG_EXAMPLE_WIFI_LISTEN_INTERVAL);
#endif

    return ESP_OK;
}

static void on_sta_start(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    configure_wifi_power_save();
}

#endif /* PS_WIFI_KNOBS_ENABLED */

#if SLAVE_LIGHT_SLEEP_ON_HOST_DEEP_SLEEP

/* Mandate non critical tasks to suspend */

#if SUSPEND_NON_CRITICAL_TASKS_WHILE_LIGHT_SLEEPING

#define MAX_SUSPENDED_TASKS 25

static TaskHandle_t suspended_tasks[MAX_SUSPENDED_TASKS];
static UBaseType_t suspended_task_count = 0;
static portMUX_TYPE suspend_mux = portMUX_INITIALIZER_UNLOCKED;

static bool is_critical_task(const char *task_name)
{
    /* List of tasks that must NEVER be suspended */
    const char *critical_tasks[] = {

        /* FreeRTOS core */
        "IDLE", "IDLE0", "IDLE1",
        "Tmr Svc", "esp_timer",
        "ipc0", "ipc1",

        /* Network */
        "tiT", "sys_evt", "eventTask",

        /* Wi-Fi */
        // "wifi",

        /* ESP-Hosted infrastructure - NEVER suspend these! */
        "sdio_rx_task",		 /* ESP-Hosted manages deinit */
        "sdio_tx_done_ta",	 /* ESP-Hosted manages deinit */
        "host_reset_task",	 /* Needed for wake */
        "ps_alert_task",    /* Gets killed automatically */

        /* Main task */
        "main",

        /* WebRTC */
        "webrtc_run",
        "esp_workq_task",
        // "flash_op_task",
        "console_repl",
        "websocket_task",
        // "pserial_task",
        // "recv_task"
    };

    /* Check if current task (can't suspend self) */
    TaskHandle_t current = xTaskGetCurrentTaskHandle();
    TaskHandle_t named = xTaskGetHandle(task_name);
    if (current == named) {
        return true;
    }

    /* Check against critical task list */
    for (int i = 0; i < sizeof(critical_tasks) / sizeof(critical_tasks[0]); i++) {
        if (strcmp(task_name, critical_tasks[i]) == 0) {
            return true;
        }
    }

    return false;
}

static void suspend_application_tasks(void)
{
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    ESP_EARLY_LOGI(TAG, "Total tasks in system: %d", task_count);

    /* Allocate array for task info */
    TaskStatus_t *task_array = pvPortMalloc(task_count * sizeof(TaskStatus_t));
    if (task_array == NULL) {
        ESP_EARLY_LOGE(TAG, "Failed to allocate task array");
        return;
    }

    /* Allocate temporary array to store tasks to suspend */
    TaskHandle_t *to_suspend = pvPortMalloc(MAX_SUSPENDED_TASKS * sizeof(TaskHandle_t));
    if (to_suspend == NULL) {
        ESP_EARLY_LOGE(TAG, "Failed to allocate suspension array");
        vPortFree(task_array);
        return;
    }

    /* Enter critical section to prevent task deletion during enumeration */
    taskENTER_CRITICAL(&suspend_mux);

    /* Get all task information
    * Pass NULL for runtime stats - works with just CONFIG_FREERTOS_USE_TRACE_FACILITY
    * No need for CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    */
    UBaseType_t actual_count = uxTaskGetSystemState(task_array, task_count, NULL);

    /* Build list of tasks to suspend while in critical section */
    UBaseType_t to_suspend_count = 0;

    for (UBaseType_t i = 0; i < actual_count && to_suspend_count < MAX_SUSPENDED_TASKS; i++) {
        const char *task_name = task_array[i].pcTaskName;
        TaskHandle_t task_handle = task_array[i].xHandle;

        /* Skip deleted tasks */
        if (task_array[i].eCurrentState == eDeleted) {
            continue;
        }

        /* Skip critical tasks */
        if (is_critical_task(task_name)) {
            ESP_EARLY_LOGD(TAG, "Skipping critical task: %s", task_name);
            continue;
        }

        /* Skip if already suspended */
        if (task_array[i].eCurrentState == eSuspended) {
            ESP_EARLY_LOGD(TAG, "Task already suspended: %s", task_name);
            continue;
        }

        /* Add to suspension list */
        to_suspend[to_suspend_count++] = task_handle;
        ESP_EARLY_LOGD(TAG, "Queuing task for suspension: %s (priority: %d, state: %d)",
                    task_name,
                    task_array[i].uxCurrentPriority,
                    task_array[i].eCurrentState);
    }

    taskEXIT_CRITICAL(&suspend_mux);

    /* Now suspend tasks outside critical section */
    suspended_task_count = 0;

    for (UBaseType_t i = 0; i < to_suspend_count; i++) {
        /* Double-check task still exists before suspending */
        eTaskState state = eTaskGetState(to_suspend[i]);
        if (state != eDeleted && state != eInvalid) {
            const char *name = pcTaskGetName(to_suspend[i]);
            ESP_EARLY_LOGI(TAG, "Suspending task: %s", name ? name : "unknown");
            vTaskSuspend(to_suspend[i]);
            suspended_tasks[suspended_task_count++] = to_suspend[i];
        } else {
            ESP_EARLY_LOGW(TAG, "Task was deleted before suspension");
        }
    }

    /* Free allocated memory */
    vPortFree(to_suspend);
    vPortFree(task_array);

    ESP_EARLY_LOGI(TAG, "Suspended %d application tasks", suspended_task_count);
}

static void resume_application_tasks(void)
{
    ESP_EARLY_LOGI(TAG, "Resuming %d suspended tasks", suspended_task_count);

    for (UBaseType_t i = 0; i < suspended_task_count; i++) {
        if (suspended_tasks[i] != NULL) {
            /* Verify task wasn't deleted while suspended */
            eTaskState state = eTaskGetState(suspended_tasks[i]);
            if (state != eDeleted && state != eInvalid) {
                const char *name = pcTaskGetName(suspended_tasks[i]);
                ESP_EARLY_LOGI(TAG, "Resuming task: %s", name ? name : "unknown");
                vTaskResume(suspended_tasks[i]);
            } else {
                ESP_EARLY_LOGW(TAG, "Task was deleted while suspended, skipping resume");
            }
            /* Clear the handle */
            suspended_tasks[i] = NULL;
        }
    }

    suspended_task_count = 0;
    ESP_EARLY_LOGI(TAG, "All application tasks resumed");
}

#endif /* SUSPEND_NON_CRITICAL_TASKS_WHILE_LIGHT_SLEEPING */

#endif /* SLAVE_LIGHT_SLEEP_ON_HOST_DEEP_SLEEP */




/* The host-sleep integration is a co-processor feature, not a chip property:
 * eh_cp_feat_host_ps is what provides the callbacks and the wake path. */
#if CONFIG_ESP_HOSTED_CP_FEAT_HOST_PS
    #define HOST_PS_INTEGRATION_ENABLED 1
    #include "host_ps_integration.h"
    #include "eh_cp_feat_host_ps_apis.h"
#else
    #define HOST_PS_INTEGRATION_ENABLED 0
#endif

/* Milliseconds to wait for the host to come back up. eh_cp_feat_host_ps caps
 * this internally; the value only bounds how long the caller blocks. */
#define HOST_WAKEUP_TIMEOUT_MS 2000

#if SLAVE_HOOKS_FOR_HOST_DEEP_SLEEP && HOST_PS_INTEGRATION_ENABLED

/* The host-sleep callbacks live in host_ps_integration.c, vendored from
 * esp_hosted's power_save example: they stop the CLI and enter CP light sleep
 * when the host sleeps, and reverse it when the host wakes. */

static void app_webrtc_event_handler(app_webrtc_event_data_t *event_data, void *user_ctx)
{
    if (event_data == NULL) {
        return;
    }

    switch (event_data->event_id) {
        /* An offer wakes the host: the signaling bridge adapter calls
         * eh_cp_feat_host_ps_wakeup_host() before it forwards one.
         * This event does not fire on the co-processor today. The bridge peer
         * connection does not report peer state, so app_webrtc never raises it.
         * The handler is kept for when it does: it would wake a host that slept
         * again before media started. On an awake host the call returns at once. */
        case APP_WEBRTC_EVENT_STREAMING_STARTED:
            ESP_LOGI(TAG, "Streaming started, waking up host");
            eh_cp_feat_host_ps_wakeup_host(HOST_WAKEUP_TIMEOUT_MS);
            break;
        default:
            break;
    }
}
#endif /* SLAVE_HOOKS_FOR_HOST_DEEP_SLEEP && HOST_PS_INTEGRATION_ENABLED */

esp_err_t power_save_init(void)
{
    esp_err_t ret = ESP_OK;

#if PS_WIFI_KNOBS_ENABLED
    /* app_main() calls this after app_network_init() and before
     * app_network_start(), so Wi-Fi is not started yet. Register for the start
     * event and apply the power-save mode there, at every start. A failure
     * costs power, not function, so it must not bring the co-processor down:
     * a reboot here takes the host's wake path with it. */
    esp_err_t evt_ret = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START,
                                                   &on_sta_start, NULL);
    if (evt_ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi power save will stay at the driver default: %s",
                 esp_err_to_name(evt_ret));
    }
#endif

#if SLAVE_HOOKS_FOR_HOST_DEEP_SLEEP && HOST_PS_INTEGRATION_ENABLED
    ESP_LOGI(TAG, "Initializing power save API");

    ret = host_ps_integration_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Host power save integration failed: %s", esp_err_to_name(ret));
    }

#else /* SLAVE_HOOKS_FOR_HOST_DEEP_SLEEP && HOST_PS_INTEGRATION_ENABLED */
    /* esp_hosted's host_power_save API is C5/C6-only (it's the slave-side
     * mechanism that lets the co-processor sleep until the host wakes it
     * over SDIO). On other targets there's no host to coordinate with —
     * skip silently. */
    ESP_LOGI(TAG, "Slave-side host power save hooks not applicable on this target");
#endif

    return ret;
}

esp_err_t power_save_register_webrtc_events(void)
{
#if SLAVE_HOOKS_FOR_HOST_DEEP_SLEEP && HOST_PS_INTEGRATION_ENABLED
    if (app_webrtc_register_event_callback(app_webrtc_event_handler, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to register WebRTC event callback for power save");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Registered WebRTC event callback for host wakeup");
#endif
    return ESP_OK;
}

