/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <inttypes.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_timer.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_connectivity.h>
#include <esp_rmaker_mqtt.h>
#include <esp_rmaker_common_events.h>
#include <esp_rmaker_work_queue.h>
#include "esp_rmaker_client_data.h"
#include "esp_rmaker_internal.h"

static const char *TAG = "esp_rmaker_connectivity";

#define CONNECTIVITY_SERVICE_NAME   "Connectivity"
#define CONNECTED_PARAM_NAME        "Connected"
#define LWT_TOPIC_BUFFER_SIZE       128
#define MAX_GROUP_ID_LENGTH         32   /* Reasonable max length for group_id */

/* Default delay before reporting Connected=true (in seconds).
 * This delay helps avoid race conditions when a device reboots quickly,
 * where the broker may trigger the old connection's LWT after the new
 * connection reports Connected=true.
 */
#ifndef CONFIG_ESP_RMAKER_CONNECTIVITY_REPORT_DELAY
#define CONFIG_ESP_RMAKER_CONNECTIVITY_REPORT_DELAY 5
#endif

typedef struct {
    esp_rmaker_device_t *service;
    esp_rmaker_param_t *connected_param;
    char *current_group_id;
    bool enabled;
    esp_timer_handle_t connected_timer;
} esp_rmaker_connectivity_priv_t;

static esp_rmaker_connectivity_priv_t *connectivity_priv;

/* LWT message for disconnect: {"Connectivity":{"Connected":false}} */
static const char *LWT_DISCONNECT_MESSAGE = "{\"Connectivity\":{\"Connected\":false}}";

static void esp_rmaker_connectivity_report_connected(void *arg)
{
    if (!connectivity_priv || !connectivity_priv->enabled) {
        return;
    }

    /* Verify MQTT is still connected before reporting */
    if (!esp_rmaker_is_mqtt_connected()) {
        ESP_LOGW(TAG, "MQTT disconnected before reporting Connected=true, skipping");
        return;
    }

    ESP_LOGI(TAG, "Reporting Connected=true");
    esp_rmaker_param_val_t val = esp_rmaker_bool(true);
    esp_rmaker_param_update_and_report(connectivity_priv->connected_param, val);
}

static esp_err_t esp_rmaker_connectivity_configure_lwt(const char *group_id)
{
    char lwt_topic[LWT_TOPIC_BUFFER_SIZE];
    const char *node_id = esp_rmaker_get_node_id();

    if (!node_id) {
        ESP_LOGE(TAG, "Node ID not available");
        return ESP_ERR_INVALID_STATE;
    }

    if (group_id && strlen(group_id) > 0) {
        snprintf(lwt_topic, sizeof(lwt_topic), "node/%s/params/local/%s", node_id, group_id);
    } else {
        snprintf(lwt_topic, sizeof(lwt_topic), "node/%s/params/local", node_id);
    }

    ESP_LOGI(TAG, "Configuring LWT topic: %s", lwt_topic);
    ESP_LOGI(TAG, "LWT message: %s", LWT_DISCONNECT_MESSAGE);
    return esp_rmaker_set_mqtt_conn_lwt(lwt_topic, LWT_DISCONNECT_MESSAGE, strlen(LWT_DISCONNECT_MESSAGE));
}

static void esp_rmaker_connectivity_mqtt_event_handler(void *arg, esp_event_base_t event_base,
                                                        int32_t event_id, void *event_data)
{
    if (!connectivity_priv || !connectivity_priv->enabled) {
        return;
    }

    if (event_base == RMAKER_COMMON_EVENT) {
        if (event_id == RMAKER_MQTT_EVENT_CONNECTED) {
            uint32_t delay_sec = CONFIG_ESP_RMAKER_CONNECTIVITY_REPORT_DELAY;
            if (delay_sec > 0 && connectivity_priv->connected_timer) {
                ESP_LOGI(TAG, "MQTT Connected - will report Connected=true after %"PRIu32" seconds", delay_sec);
                /* Start timer to report Connected=true after delay.
                 * This delay helps avoid race with LWT from duplicate client ID on reboot.
                 */
                esp_timer_stop(connectivity_priv->connected_timer);  /* Stop if already running */
                esp_timer_start_once(connectivity_priv->connected_timer, delay_sec * 1000000);
            } else {
                /* No delay configured, report immediately */
                ESP_LOGI(TAG, "MQTT Connected - reporting Connected=true immediately");
                esp_rmaker_connectivity_report_connected(NULL);
            }

        } else if (event_id == RMAKER_MQTT_EVENT_DISCONNECTED) {
            ESP_LOGD(TAG, "MQTT Disconnected");
            /* Stop the connected timer if running - don't report Connected=true if we disconnect */
            if (connectivity_priv->connected_timer) {
                esp_timer_stop(connectivity_priv->connected_timer);
            }
            /* Note: We don't report Connected=false here because:
             * 1. We can't publish when disconnected
             * 2. The LWT will automatically publish the disconnect message
             */
        }
    }
}

/* Work queue callback to perform MQTT config update and reconnection.
 * This is needed because MQTT cannot be stopped from within its own task/callback context.
 * Uses update_config which preserves subscriptions across reconnection.
 */
static void esp_rmaker_connectivity_reconnect_work_cb(void *priv_data)
{
    ESP_LOGI(TAG, "Performing deferred MQTT config update");

    /* Get fresh conn_params which will include the updated LWT */
    esp_rmaker_mqtt_conn_params_t *conn_params = esp_rmaker_get_mqtt_conn_params();
    if (!conn_params) {
        ESP_LOGE(TAG, "Failed to get MQTT connection params");
        return;
    }

    esp_err_t err = esp_rmaker_mqtt_update_config(conn_params);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update MQTT config: %d", err);
    }
}

/* Helper to compare group_ids (handles NULL and empty strings and validates length)
 */
static bool esp_rmaker_connectivity_group_id_matches(const char *new_id)
{
    const char *current = connectivity_priv->current_group_id;

    /* Both NULL or empty -> match */
    bool new_empty = (!new_id || strlen(new_id) == 0);
    bool current_empty = (!current || strlen(current) == 0);

    if (new_empty && current_empty) {
        return true;
    }
    if (new_empty != current_empty) {
        return false;
    }

    /* Both non-empty - validate new_id length to catch suspicious data.
     * Note: current is trusted since it was set by our own code.
     */
    size_t new_len = strlen(new_id);
    if (new_len > MAX_GROUP_ID_LENGTH) {
        ESP_LOGW(TAG, "Group ID length exceeds maximum (%d), treating as mismatch", MAX_GROUP_ID_LENGTH);
        return false;
    }

    /* Both valid non-empty strings, compare */
    return (strcmp(new_id, current) == 0);
}

esp_err_t esp_rmaker_connectivity_update_lwt(const char *group_id)
{
    if (!connectivity_priv || !connectivity_priv->enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Check if group_id has actually changed */
    if (esp_rmaker_connectivity_group_id_matches(group_id)) {
        ESP_LOGD(TAG, "Group ID unchanged, skipping LWT update");
        return ESP_OK;
    }

    /* Update stored group_id */
    if (connectivity_priv->current_group_id) {
        free(connectivity_priv->current_group_id);
        connectivity_priv->current_group_id = NULL;
    }

    if (group_id && strlen(group_id) > 0) {
        size_t group_id_len = strlen(group_id);
        if (group_id_len > MAX_GROUP_ID_LENGTH) {
            ESP_LOGE(TAG, "Group ID length (%d) exceeds maximum (%d)", group_id_len, MAX_GROUP_ID_LENGTH);
            return ESP_ERR_INVALID_ARG;
        }
        connectivity_priv->current_group_id = strdup(group_id);
        if (!connectivity_priv->current_group_id) {
            ESP_LOGE(TAG, "Failed to allocate memory for group_id");
            return ESP_ERR_NO_MEM;
        }
    }

    /* Configure new LWT data */
    esp_err_t err = esp_rmaker_connectivity_configure_lwt(group_id);
    if (err != ESP_OK) {
        return err;
    }

    /* If MQTT is connected, we need to reconnect with new LWT.
     * LWT can only be set during MQTT CONNECT phase.
     * Use work queue to defer reconnection since this may be called from MQTT task context.
     */
    if (esp_rmaker_is_mqtt_connected()) {
        ESP_LOGI(TAG, "Scheduling MQTT reconnection to apply new LWT settings");
        err = esp_rmaker_work_queue_add_task(esp_rmaker_connectivity_reconnect_work_cb, NULL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to schedule MQTT reconnection: %d", err);
            return err;
        }
    }

    return ESP_OK;
}

bool esp_rmaker_connectivity_is_enabled(void)
{
    return (connectivity_priv && connectivity_priv->enabled);
}

esp_err_t esp_rmaker_connectivity_enable(void)
{
    if (connectivity_priv) {
        ESP_LOGW(TAG, "Connectivity service already enabled");
        return ESP_OK;
    }

    connectivity_priv = calloc(1, sizeof(esp_rmaker_connectivity_priv_t));
    if (!connectivity_priv) {
        ESP_LOGE(TAG, "Failed to allocate memory for connectivity private data");
        return ESP_ERR_NO_MEM;
    }

    /* Create the Connectivity service */
    connectivity_priv->service = esp_rmaker_service_create(CONNECTIVITY_SERVICE_NAME,
                                                           ESP_RMAKER_SERVICE_CONNECTIVITY,
                                                           NULL);
    if (!connectivity_priv->service) {
        ESP_LOGE(TAG, "Failed to create Connectivity service");
        free(connectivity_priv);
        connectivity_priv = NULL;
        return ESP_FAIL;
    }

    /* Check MQTT connection status to initialize Connected parameter with correct value */
    bool mqtt_connected = esp_rmaker_is_mqtt_connected();

    /* Create the Connected parameter with correct initial value */
    connectivity_priv->connected_param = esp_rmaker_param_create(CONNECTED_PARAM_NAME,
                                                                  ESP_RMAKER_PARAM_CONNECTED,
                                                                  esp_rmaker_bool(mqtt_connected),
                                                                  PROP_FLAG_READ);
    if (!connectivity_priv->connected_param) {
        ESP_LOGE(TAG, "Failed to create Connected param");
        esp_rmaker_device_delete(connectivity_priv->service);
        free(connectivity_priv);
        connectivity_priv = NULL;
        return ESP_FAIL;
    }

    /* Add param to service */
    esp_err_t err = esp_rmaker_device_add_param(connectivity_priv->service, connectivity_priv->connected_param);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add Connected param to service");
        esp_rmaker_device_delete(connectivity_priv->service);
        free(connectivity_priv);
        connectivity_priv = NULL;
        return err;
    }

    /* Add service to node */
    err = esp_rmaker_node_add_device(esp_rmaker_get_node(), connectivity_priv->service);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add Connectivity service to node");
        esp_rmaker_device_delete(connectivity_priv->service);
        free(connectivity_priv);
        connectivity_priv = NULL;
        return err;
    }

    /* Create timer for delayed Connected=true reporting */
    esp_timer_create_args_t timer_args = {
        .callback = esp_rmaker_connectivity_report_connected,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "connectivity_timer"
    };
    err = esp_timer_create(&timer_args, &connectivity_priv->connected_timer);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create connected timer: %d. Will report immediately.", err);
        connectivity_priv->connected_timer = NULL;
        /* Continue anyway - will just report immediately */
    }

    /* Register for MQTT events */
    err = esp_event_handler_register(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_CONNECTED,
                                     &esp_rmaker_connectivity_mqtt_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT connected event handler");
        /* Continue anyway - service will still work, just won't auto-update */
    }

    err = esp_event_handler_register(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_DISCONNECTED,
                                     &esp_rmaker_connectivity_mqtt_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT disconnected event handler");
        /* Continue anyway */
    }

    /* Get persisted group_id if available */
    char *stored_group_id = NULL;
    err = esp_rmaker_get_stored_group_id(&stored_group_id);
    if (err == ESP_OK && stored_group_id) {
        /* Validate length before storing */
        size_t stored_len = strlen(stored_group_id);
        if (stored_len > MAX_GROUP_ID_LENGTH) {
            ESP_LOGW(TAG, "Stored group_id length (%d) exceeds maximum (%d), ignoring", stored_len, MAX_GROUP_ID_LENGTH);
            free(stored_group_id);
        } else {
            /* Use stored_group_id directly - transfer ownership to connectivity_priv */
            connectivity_priv->current_group_id = stored_group_id;
            ESP_LOGI(TAG, "Found persisted group_id: %s", stored_group_id);
        }
    }

    /* Configure initial LWT with persisted group_id (or NULL if none) */
    err = esp_rmaker_connectivity_configure_lwt(connectivity_priv->current_group_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to configure LWT: %d. LWT may not be active.", err);
        /* Continue anyway - service will still work */
    } else {
        /* Reinit MQTT to pick up the LWT configuration.
         * This is needed because MQTT was already initialized in esp_rmaker_node_init()
         * before the connectivity service was enabled.
         */
        err = esp_rmaker_mqtt_reinit_with_new_params();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to reinit MQTT with LWT: %d. LWT may not be active.", err);
            /* Continue anyway - service will still work */
        }
    }

    connectivity_priv->enabled = true;
    ESP_LOGI(TAG, "Connectivity service enabled");
    return ESP_OK;
}
