/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/task.h>
#include <esp_efuse.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_wifi_types.h>
#include <esp_wifi.h>
#include <nvs.h>
#include <json_parser.h>
#ifdef CONFIG_ESP_RMAKER_OTA_USE_HTTPS
#include "esp_rmaker_https_ota.h"
#endif
#ifdef CONFIG_ESP_RMAKER_OTA_USE_MQTT
#include "esp_rmaker_mqtt_ota.h"
#endif
#include <errno.h>

#include <esp_rmaker_utils.h>
#include <esp_rmaker_common_events.h>
#include <esp_rmaker_utils.h>
#include "esp_rmaker_internal.h"
#include "esp_rmaker_ota_internal.h"

/* Forward declarations for static functions */
static esp_err_t esp_rmaker_ota_handle_metadata_common(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data);
static esp_err_t esp_rmaker_ota_success_reboot_sequence(esp_rmaker_ota_handle_t ota_handle, const char *protocol_name, int attempt_count);


#ifdef CONFIG_ESP_RMAKER_USE_CERT_BUNDLE
#define ESP_RMAKER_USE_CERT_BUNDLE
#include <esp_crt_bundle.h>
#endif
static const char *TAG = "esp_rmaker_ota";

/* OTA reboot timer and NVS constants */
#define OTA_REBOOT_TIMER_SEC    10
#define ESP_RMAKER_NVS_PART_NAME             "nvs"
#define RMAKER_OTA_UPDATE_FLAG_NVS_NAME      "ota_update"

/* OTA retry constants */
#ifndef ESP_RMAKER_OTA_MAX_RETRIES
#define ESP_RMAKER_OTA_MAX_RETRIES   CONFIG_ESP_RMAKER_OTA_MAX_RETRIES
#endif
#ifndef ESP_RMAKER_OTA_RETRY_DELAY_SECONDS
#define ESP_RMAKER_OTA_RETRY_DELAY_SECONDS   (CONFIG_ESP_RMAKER_OTA_RETRY_DELAY_MINUTES * 60)
#endif

/* Core OTA rollback functionality */
#define RMAKER_OTA_ROLLBACK_WAIT_PERIOD    CONFIG_ESP_RMAKER_OTA_ROLLBACK_WAIT_PERIOD

ESP_EVENT_DEFINE_BASE(RMAKER_OTA_EVENT);

static esp_rmaker_ota_t *g_ota_priv;


char *esp_rmaker_ota_status_to_string(ota_status_t status)
{
    switch (status) {
        case OTA_STATUS_IN_PROGRESS:
            return "in-progress";
        case OTA_STATUS_SUCCESS:
            return "success";
        case OTA_STATUS_FAILED:
            return "failed";
        case OTA_STATUS_DELAYED:
            return "delayed";
        case OTA_STATUS_REJECTED:
            return "rejected";
        default:
            return "invalid";
    }
    return "invalid";
}

esp_rmaker_ota_event_t esp_rmaker_ota_status_to_event(ota_status_t status)
{
    switch (status) {
        case OTA_STATUS_IN_PROGRESS:
            return RMAKER_OTA_EVENT_IN_PROGRESS;
        case OTA_STATUS_SUCCESS:
            return RMAKER_OTA_EVENT_SUCCESSFUL;
        case OTA_STATUS_FAILED:
            return RMAKER_OTA_EVENT_FAILED;
        case OTA_STATUS_DELAYED:
            return RMAKER_OTA_EVENT_DELAYED;
        case OTA_STATUS_REJECTED:
            return RMAKER_OTA_EVENT_REJECTED;
        default:
            ESP_LOGD(TAG, "No Rmaker OTA Event for given status: %d: %s",
                    status, esp_rmaker_ota_status_to_string(status));
    }
    return RMAKER_OTA_EVENT_INVALID;
}

esp_err_t esp_rmaker_ota_post_event(esp_rmaker_event_t event_id, void *data, size_t data_size)
{
    return esp_event_post(RMAKER_OTA_EVENT, event_id, data, data_size, portMAX_DELAY);
}

esp_err_t esp_rmaker_ota_report_status(esp_rmaker_ota_handle_t ota_handle, ota_status_t status, char *additional_info)
{
    ESP_LOGI(TAG, "Reporting %s: %s", esp_rmaker_ota_status_to_string(status), additional_info);

    if (!ota_handle) {
        return ESP_FAIL;
    }
    esp_rmaker_ota_t *ota = (esp_rmaker_ota_t *)ota_handle;
    esp_err_t err = ESP_FAIL;
    if (ota->type == OTA_USING_PARAMS) {
        err = esp_rmaker_ota_report_status_using_params(ota_handle, status, additional_info);
    } else if (ota->type == OTA_USING_TOPICS) {
        err = esp_rmaker_ota_report_status_using_topics(ota_handle, status, additional_info);
    }
    if (err == ESP_OK) {
        esp_rmaker_ota_t *ota = (esp_rmaker_ota_t *)ota_handle;
        ota->last_reported_status = status;
    }
    esp_rmaker_ota_post_event(esp_rmaker_ota_status_to_event(status), additional_info, strlen(additional_info) + 1);
    return err;
}

void esp_rmaker_ota_common_cb(void *priv)
{
    if (!priv) {
        return;
    }
    esp_rmaker_ota_t *ota = (esp_rmaker_ota_t *)priv;
    if (!ota->url) {
        goto ota_finish;
    }
    esp_rmaker_ota_data_t ota_data = {
        .url = ota->url,
#ifdef CONFIG_ESP_RMAKER_OTA_USE_MQTT
        .stream_id = ota->stream_id,
#endif
        .filesize = ota->filesize,
        .fw_version = ota->fw_version,
        .file_md5 = ota->file_md5,
        .ota_job_id = (char *)ota->transient_priv,
        .server_cert = ota->server_cert,
        .priv = ota->priv,
        .metadata = ota->metadata
    };
    ota->ota_cb((esp_rmaker_ota_handle_t) ota, &ota_data);
ota_finish:
    if (ota->type == OTA_USING_PARAMS) {
        esp_rmaker_ota_finish_using_params(ota);
    } else if (ota->type == OTA_USING_TOPICS) {
        esp_rmaker_ota_finish_using_topics(ota);
    }
}

esp_err_t validate_image_header(esp_rmaker_ota_handle_t ota_handle,
        esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGD(TAG, "Running firmware version: %s", running_app_info.version);
    }

#ifndef CONFIG_ESP_RMAKER_SKIP_PROJECT_NAME_CHECK
    if (memcmp(new_app_info->project_name, running_app_info.project_name, sizeof(new_app_info->project_name)) != 0) {
        ESP_LOGW(TAG, "OTA Image built for Project: %s. Expected: %s",
                new_app_info->project_name, running_app_info.project_name);
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_REJECTED, "Project Name mismatch");
        return ESP_FAIL;
    }
#endif

#ifndef CONFIG_ESP_RMAKER_SKIP_VERSION_CHECK
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0) {
        ESP_LOGW(TAG, "Current running version is same as the new. We will not continue the update.");
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_REJECTED, "Same version received");
        return ESP_FAIL;
    }
#endif

#ifndef CONFIG_ESP_RMAKER_SKIP_SECURE_VERSION_CHECK
    if (esp_efuse_check_secure_version(new_app_info->secure_version) == false) {
        ESP_LOGW(TAG, "New secure version is lower than stored in efuse. We will not continue the update.");
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_REJECTED, "Lower secure version received");
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

/* Common retry loop with configurable protocol-specific logic */

static esp_err_t esp_rmaker_ota_retry_loop(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data,
                                   ota_protocol_func_t protocol_func, const char *protocol_name, int *attempt_count)
{
    esp_err_t err = ESP_FAIL;
    char err_desc[128] = {0};
    int attempt;

    for (attempt = 0; attempt < ESP_RMAKER_OTA_MAX_RETRIES; ++attempt) {
        /* Simplified status reporting - just say "OTA" for cleaner messages */
        char info[64];
        snprintf(info, sizeof(info), "Starting OTA Upgrade (%s attempt %d/%d)", protocol_name, attempt + 1, ESP_RMAKER_OTA_MAX_RETRIES);
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_IN_PROGRESS, info);
        ESP_LOGW(TAG, "Starting %s OTA attempt %d/%d. This may take time.", protocol_name, attempt + 1, ESP_RMAKER_OTA_MAX_RETRIES);

        err = protocol_func(ota_handle, ota_data, err_desc, sizeof(err_desc));
        if (err == ESP_OK) {
            break;
        } else if (err == ESP_ERR_INVALID_STATE) {
            return ESP_FAIL;
        } else {
            ESP_LOGE(TAG, "%s OTA attempt %d failed: %s", protocol_name, attempt + 1, err_desc);
            /* Simplified failure reporting */
            char fail_info[192];
            snprintf(fail_info, sizeof(fail_info), "OTA Attempt %d/%d failed: %s", attempt + 1, ESP_RMAKER_OTA_MAX_RETRIES, err_desc);
            esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, fail_info);
        }
    }

    if (err != ESP_OK) {
        /* Handle retry delay for topic-based OTA */
        esp_rmaker_ota_t *ota = (esp_rmaker_ota_t *)ota_handle;
        if (ota->type == OTA_USING_TOPICS) {
            esp_rmaker_ota_fetch_with_delay(ESP_RMAKER_OTA_RETRY_DELAY_SECONDS);
        }
        return ESP_FAIL;
    }

    *attempt_count = attempt + 1;
    return ESP_OK;
}

/* Complete OTA workflow orchestration function */

esp_err_t esp_rmaker_ota_start_workflow(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data,
                                       ota_protocol_func_t protocol_func, const char *protocol_name)
{
    /* Step 1: Handle metadata */
    esp_err_t metadata_result = esp_rmaker_ota_handle_metadata_common(ota_handle, ota_data);
    if (metadata_result != ESP_OK) {
        return metadata_result;
    }

    /* Step 2: Post starting event */
    esp_rmaker_ota_post_event(RMAKER_OTA_EVENT_STARTING, NULL, 0);

    /* Step 3: Execute retry loop */
    int attempt_count = 0;
    esp_err_t retry_result = esp_rmaker_ota_retry_loop(ota_handle, ota_data, protocol_func, protocol_name, &attempt_count);
    if (retry_result != ESP_OK) {
        return ESP_FAIL;
    }

    /* Step 4: Handle success and reboot */
    return esp_rmaker_ota_success_reboot_sequence(ota_handle, protocol_name, attempt_count);
}

#ifdef CONFIG_ESP_RMAKER_OTA_TIME_SUPPORT

/* Retry delay for cases wherein time info itself is not available */
#define OTA_FETCH_RETRY_DELAY   30
#define MINUTES_IN_DAY          (24 * 60)
#define OTA_DELAY_TIME_BUFFER   5

/* Check if time data is available in the metadata. Format
 * {"download_window":{"end":1155,"start":1080},"validity":{"end":1665426600,"start":1665081000}}
 */
esp_rmaker_ota_action_t esp_rmaker_ota_handle_time(jparse_ctx_t *jptr, esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data)
{
    bool time_info = false;
    int start_min = -1, end_min = -1, start_date = -1, end_date = -1;
    if (json_obj_get_object(jptr, "download_window") == 0) {
        /* Download window means specific time of day. Eg, Between 02:00am and 05:00am only */
        time_info = true;
        json_obj_get_int(jptr, "start", &start_min);
        json_obj_get_int(jptr, "end", &end_min);
        json_obj_leave_object(jptr);
        ESP_LOGI(TAG, "Download Window : %d %d", start_min, end_min);
    }
    if (json_obj_get_object(jptr, "validity") == 0) {
        /* Validity indicates start and end epoch time, typicaly useful if OTA is to be performed between some dates */
        time_info = true;
        json_obj_get_int(jptr, "start", &start_date);
        json_obj_get_int(jptr, "end", &end_date);
        json_obj_leave_object(jptr);
        ESP_LOGI(TAG, "Validity : %d %d", start_date, end_date);
    }
    if (time_info) {
        /* If time info is present, but time is not yet synchronised, we will re-fetch OTA after some time */
        if (esp_rmaker_time_check() != true) {
            esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_DELAYED, "No time information available yet.");
            esp_rmaker_ota_fetch_with_delay(OTA_FETCH_RETRY_DELAY);
            return OTA_DELAYED;
        }
        time_t current_timestamp = 0;
        struct tm current_time = {0};
        time(&current_timestamp);
        localtime_r(&current_timestamp, &current_time);

        /* Check for date validity first */
        if ((start_date != -1) && (current_timestamp < start_date)) {
            int delay_time = start_date - current_timestamp;
            /* The delay logic here can include the start_min and end_min as well, but it makes the logic quite complex,
             * just for a minor optimisation.
             */
            ESP_LOGI(TAG, "Delaying OTA by %d seconds (%d min) as it is not valid yet.", delay_time, delay_time / 60);
            esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_DELAYED, "Not within valid window.");
            esp_rmaker_ota_fetch_with_delay(delay_time + OTA_DELAY_TIME_BUFFER);
            return OTA_DELAYED;
        } else if ((end_date != -1) && (current_timestamp > end_date)) {
            ESP_LOGE(TAG, "OTA download window lapsed");
            esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "OTA download window lapsed.");
            return OTA_ERR;
        }

        /* Check for download window */
        if (start_min != -1) {
            /* end_min is required if start_min is provided */
            if (end_min == -1) {
                ESP_LOGE(TAG, "Download window should have an end time if start time is specified.");
                esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Invalid download window specified.");
                return OTA_ERR;
            }
            int cur_min = current_time.tm_hour * 60 + current_time.tm_min;
            if (start_min > end_min) {
                /* This means that the window is across midnight (Eg. 23:00 to 02:00 i.e. 1380 to 120).
                 * We are just moving the window here such that start_min becomes 0 and the comparisons are simplified.
                 * For this example, diff_min will be  1440 - 1380 = 60.
                 * Effective end_min: 180
                 * If cur_time is 18:00, effective cur_time = 1080 + 60 = 1140
                 * If cur_time is 23:30, effective cur_time = 1410 + 60 = 1470 ( > MINUTES_IN_DAY)
                 *          So, cur_time = 1470 - 1440 = 30
                 * */
                int diff_min = MINUTES_IN_DAY - start_min;
                start_min = 0;
                end_min += diff_min;
                cur_min += diff_min;
                if (cur_min >= MINUTES_IN_DAY) {
                    cur_min -= MINUTES_IN_DAY;
                }
            }
            /* Current time is within OTA download window */
            if ((cur_min >= start_min) && (cur_min <= end_min)) {
                ESP_LOGI(TAG, "OTA received within download window.");
                return OTA_OK;
            } else {
                /* Delay the OTA if it is not in the download window. Even if it later goes outside the valid date range,
                 * that will be handled in subsequent ota fetch. Reporting failure here itself would mark the OTA job
                 * as failed and the node will no more get the OTA even if it tries to fetch it again due to a reboot or
                 * other action within the download window.
                 */
                int delay_min = start_min - cur_min;
                if (delay_min < 0) {
                    delay_min += MINUTES_IN_DAY;
                }
                ESP_LOGI(TAG, "Delaying OTA by %d seconds (%d min) as it is not within download window.", delay_min * 60, delay_min);
                esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_DELAYED, "Not within download window.");
                esp_rmaker_ota_fetch_with_delay(delay_min * 60 + OTA_DELAY_TIME_BUFFER);
                return OTA_DELAYED;
            }
        } else {
            ESP_LOGI(TAG, "OTA received within validity period.");
        }
    }
    return OTA_OK;
}

#endif /* CONFIG_ESP_RMAKER_OTA_TIME_SUPPORT */

static esp_rmaker_ota_action_t esp_rmaker_ota_handle_metadata(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data)
{
    if (!ota_data->metadata) {
        return OTA_OK;
    }
    esp_rmaker_ota_action_t ota_action = OTA_OK;
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, ota_data->metadata, strlen(ota_data->metadata)) == 0) {
#ifdef CONFIG_ESP_RMAKER_OTA_TIME_SUPPORT
        /* Handle OTA timing data, if any */
        ota_action = esp_rmaker_ota_handle_time(&jctx, ota_handle, ota_data);
#endif /* CONFIG_ESP_RMAKER_OTA_TIME_SUPPORT */
        json_parse_end(&jctx);
    }
    return ota_action;
}

/* Common OTA callback helper functions */

static esp_err_t esp_rmaker_ota_handle_metadata_common(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data)
{
    /* Handle OTA metadata, if any */
    if (ota_data->metadata) {
        esp_rmaker_ota_action_t metadata_result = esp_rmaker_ota_handle_metadata(ota_handle, ota_data);
        if (metadata_result != OTA_OK) {
            ESP_LOGW(TAG, "Cannot proceed with the OTA as per the metadata received.");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static esp_err_t esp_rmaker_ota_success_reboot_sequence(esp_rmaker_ota_handle_t ota_handle, const char *protocol_name, int attempt_count)
{
    /* Success path: rest of the reboot/rollback logic */
#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    nvs_handle handle;
    esp_err_t nvs_err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, RMAKER_OTA_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (nvs_err == ESP_OK) {
        uint8_t ota_update = 1;
        nvs_set_blob(handle, RMAKER_OTA_UPDATE_FLAG_NVS_NAME, &ota_update, sizeof(ota_update));
        nvs_close(handle);
    }
    char reboot_info[80];
    snprintf(reboot_info, sizeof(reboot_info), "Rebooting into new firmware (after %d %s attempt%s)",
             attempt_count, protocol_name, (attempt_count == 1) ? "" : "s");
    esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_IN_PROGRESS, reboot_info);
#else
    char success_info[80];
    snprintf(success_info, sizeof(success_info), "%s OTA Upgrade finished successfully (after %d attempt%s)",
             protocol_name, attempt_count, (attempt_count == 1) ? "" : "s");
    esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_SUCCESS, success_info);
#endif
#ifndef CONFIG_ESP_RMAKER_OTA_DISABLE_AUTO_REBOOT
    ESP_LOGI(TAG, "%s OTA upgrade successful. Rebooting in %d seconds...", protocol_name, OTA_REBOOT_TIMER_SEC);
    esp_rmaker_reboot(OTA_REBOOT_TIMER_SEC);
#else
    ESP_LOGI(TAG, "%s OTA upgrade successful. Auto reboot is disabled. Requesting a Reboot via Event handler.", protocol_name);
    esp_rmaker_ota_post_event(RMAKER_OTA_EVENT_REQ_FOR_REBOOT, NULL, 0);
#endif
    return ESP_OK;
}

/* Local macro for OTA max retries, can be overridden for development */
#ifndef ESP_RMAKER_OTA_MAX_RETRIES
#define ESP_RMAKER_OTA_MAX_RETRIES   CONFIG_ESP_RMAKER_OTA_MAX_RETRIES
#endif

/* Local macro for OTA retry delay in seconds, can be overridden for development */
#ifndef ESP_RMAKER_OTA_RETRY_DELAY_SECONDS
#define ESP_RMAKER_OTA_RETRY_DELAY_SECONDS   (CONFIG_ESP_RMAKER_OTA_RETRY_DELAY_MINUTES * 60)
#endif


esp_err_t esp_rmaker_ota_default_cb(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data)
{
#ifdef CONFIG_ESP_RMAKER_OTA_USE_HTTPS
    return esp_rmaker_ota_https_cb(ota_handle, ota_data);
#else /* CONFIG_ESP_RMAKER_OTA_USE_MQTT */
    return esp_rmaker_ota_mqtt_cb(ota_handle, ota_data);
#endif
}

static void event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    esp_rmaker_ota_t *ota = (esp_rmaker_ota_t *)arg;
    esp_rmaker_ota_diag_status_t diag_status = OTA_DIAG_STATUS_SUCCESS;
    if (ota->ota_diag) {
        esp_rmaker_ota_diag_priv_t ota_diag_priv = {
            .state = OTA_DIAG_STATE_POST_MQTT,
            .rmaker_ota = ota->validation_in_progress
        };
        diag_status = ota->ota_diag(&ota_diag_priv, ota->priv);
    }
    if (diag_status == OTA_DIAG_STATUS_SUCCESS) {
        esp_rmaker_ota_mark_valid();
    } else if (diag_status == OTA_DIAG_STATUS_FAIL) {
        ESP_LOGE(TAG, "Diagnostics failed! Start rollback to the previous version ...");
        esp_rmaker_ota_mark_invalid();
    } else {
        ESP_LOGW(TAG, "Waiting for application to validate OTA.");
    }
}

static esp_err_t esp_rmaker_erase_rollback_flag(void)
{
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, RMAKER_OTA_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_key(handle, RMAKER_OTA_UPDATE_FLAG_NVS_NAME);
        nvs_commit(handle);
        nvs_close(handle);
    }
    return ESP_OK;
}

esp_err_t esp_rmaker_ota_mark_valid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state != ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGW(TAG, "OTA Already marked as valid");
            return ESP_ERR_INVALID_STATE;
        }
    }

    esp_event_handler_unregister(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_CONNECTED, &event_handler);
    esp_rmaker_ota_t *ota = g_ota_priv;
    if (!ota) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_ota_mark_app_valid_cancel_rollback();
    esp_rmaker_erase_rollback_flag();
    ota->ota_in_progress = false;
    if (ota->rollback_timer) {
        xTimerStop(ota->rollback_timer, portMAX_DELAY);
        xTimerDelete(ota->rollback_timer, portMAX_DELAY);
        ota->rollback_timer = NULL;
    }
    esp_rmaker_ota_report_status((esp_rmaker_ota_handle_t )ota, OTA_STATUS_SUCCESS, "OTA Upgrade finished and verified successfully");
    if (ota->type == OTA_USING_TOPICS) {
        if (esp_rmaker_ota_fetch_with_delay(RMAKER_OTA_FETCH_DELAY) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create OTA Fetch timer.");
        }
    }
    return ESP_OK;
}

esp_err_t esp_rmaker_ota_mark_invalid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state != ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGE(TAG, "Cannot rollback due to invalid OTA state.");
            return ESP_ERR_INVALID_STATE;
        }
    }
    esp_ota_mark_app_invalid_rollback_and_reboot();
    return ESP_OK;
}


static void esp_ota_rollback(TimerHandle_t handle)
{
    ESP_LOGE(TAG, "Could not verify firmware even after %d seconds since boot-up. Rolling back.",
            RMAKER_OTA_ROLLBACK_WAIT_PERIOD);
    esp_rmaker_ota_mark_invalid();
}

static esp_err_t esp_ota_check_for_mqtt(esp_rmaker_ota_t *ota)
{
    ota->rollback_timer = xTimerCreate("ota_rollback_tm", (RMAKER_OTA_ROLLBACK_WAIT_PERIOD * 1000) / portTICK_PERIOD_MS,
                            pdTRUE, NULL, esp_ota_rollback);
    if (ota->rollback_timer) {
        xTimerStart(ota->rollback_timer, 0);
    } else {
        ESP_LOGW(TAG, "Could not create rollback timer. Will require manual reboot if firmware verification fails");
    }

    return esp_event_handler_register(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_CONNECTED, &event_handler, ota);
}

static void esp_rmaker_ota_manage_rollback(esp_rmaker_ota_t *ota)
{
    /* If rollback is enabled, and the ota update flag is found, it means that the OTA validation is pending
    */
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, RMAKER_OTA_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        uint8_t ota_update = 0;
        size_t len = sizeof(ota_update);
        if ((err = nvs_get_blob(handle, RMAKER_OTA_UPDATE_FLAG_NVS_NAME, &ota_update, &len)) == ESP_OK) {
            ota->validation_in_progress = true;
        }
        nvs_close(handle);
    }
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        ESP_LOGI(TAG, "OTA state = %d", ota_state);
        /* Not checking for CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE here because the firmware may have
         * it disabled, but bootloader may have it enabled, in which case, we will have to
         * handle this state.
         */
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "First Boot after an OTA");
            /* Run diagnostic function */
            esp_rmaker_ota_diag_status_t diag_status = OTA_DIAG_STATUS_SUCCESS;
            if (ota->ota_diag) {
                esp_rmaker_ota_diag_priv_t ota_diag_priv = {
                    .state = OTA_DIAG_STATE_INIT,
                    .rmaker_ota = ota->validation_in_progress
                };
                diag_status = ota->ota_diag(&ota_diag_priv, ota->priv);
            }
            if (diag_status != OTA_DIAG_STATUS_FAIL) {
                ESP_LOGI(TAG, "Diagnostics completed successfully! Continuing execution ...");
                /* Will not mark the image valid here immediately, but instead will wait for
                 * MQTT connection. The below flag will tell the OTA functions that the earlier
                 * OTA is still in progress.
                 */
                ota->ota_in_progress = true;
                esp_ota_check_for_mqtt(ota);
            } else {
                ESP_LOGE(TAG, "Diagnostics failed! Start rollback to the previous version ...");
                esp_rmaker_ota_mark_invalid();
            }
        } else {
            /* If rollback is enabled, and the ota update flag is found, it means that the firmware was rolled back
            */
            if (ota->validation_in_progress) {
                ota->rolled_back = true;
                esp_rmaker_erase_rollback_flag();
                if (ota->type == OTA_USING_PARAMS) {
                    /* Calling this only for OTA_USING_PARAMS, because for OTA_USING_TOPICS,
                     * the work queue function will manage the status reporting later.
                     */
                    esp_rmaker_ota_report_status((esp_rmaker_ota_handle_t )ota,
                            OTA_STATUS_REJECTED, "Firmware rolled back");
                }
            }
        }
    }
}

/* Protocol-agnostic default config - no protocol-specific dependencies */
static const esp_rmaker_ota_config_t ota_default_config = {
    .server_cert = NULL,  /* Each protocol handles its own certificate defaults */
    .priv = NULL,
};

/* Enable the ESP RainMaker specific OTA */
esp_err_t esp_rmaker_ota_enable(esp_rmaker_ota_config_t *ota_config, esp_rmaker_ota_type_t type)
{
    if (ota_config == NULL) {
        ota_config = (esp_rmaker_ota_config_t *)&ota_default_config;
    }
    if ((type != OTA_USING_PARAMS) && (type != OTA_USING_TOPICS)) {
        ESP_LOGE(TAG,"Invalid arguments for esp_rmaker_ota_enable()");
        return ESP_ERR_INVALID_ARG;
    }
    static bool ota_init_done;
    if (ota_init_done) {
        ESP_LOGE(TAG, "OTA already initialised");
        return ESP_FAIL;
    }
    esp_rmaker_ota_t *ota = MEM_CALLOC_EXTRAM(1, sizeof(esp_rmaker_ota_t));
    if (!ota) {
        ESP_LOGE(TAG, "Failed to allocate memory for esp_rmaker_ota_t");
        return ESP_ERR_NO_MEM;
    }
    if (ota_config->ota_cb) {
        ota->ota_cb = ota_config->ota_cb;
    } else {
        ota->ota_cb = esp_rmaker_ota_default_cb;
    }
    ota->ota_diag = ota_config->ota_diag;
    ota->priv = ota_config->priv;
    ota->server_cert = ota_config->server_cert;
    esp_err_t err = ESP_FAIL;
    ota->type = type;
    if (type == OTA_USING_PARAMS) {
        err = esp_rmaker_ota_enable_using_params(ota);
    } else if (type == OTA_USING_TOPICS) {
        err = esp_rmaker_ota_enable_using_topics(ota);
    }
    if (err == ESP_OK) {
        esp_rmaker_ota_manage_rollback(ota);
        ota_init_done = true;
        g_ota_priv = ota;
    } else {
        free(ota);
        ESP_LOGE(TAG, "Failed to enable OTA");
    }
#ifdef CONFIG_ESP_RMAKER_OTA_TIME_SUPPORT
    esp_rmaker_time_sync_init(NULL);
#endif
    return err;
}

esp_err_t esp_rmaker_ota_enable_default(void)
{
    return esp_rmaker_ota_enable(NULL, OTA_USING_TOPICS);
}
