/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include <json_parser.h>
#include <json_generator.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs.h>
#include <string.h>
#include <esp_rmaker_work_queue.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_ota.h>
#include <esp_rmaker_utils.h>
#include <esp_rmaker_common_events.h>
#include <time.h>

#include "esp_rmaker_internal.h"
#include "esp_rmaker_ota_internal.h"
#include "esp_rmaker_mqtt.h"
#include "esp_rmaker_mqtt_topics.h"

#ifdef CONFIG_ESP_RMAKER_OTA_AUTOFETCH
/* Use FreeRTOS timer instead */
static TimerHandle_t ota_autofetch_timer;
/* Autofetch period in hours */
#define OTA_AUTOFETCH_PERIOD   CONFIG_ESP_RMAKER_OTA_AUTOFETCH_PERIOD
/* Convert hours to milliseconds for FreeRTOS timer */
#define OTA_AUTOFETCH_PERIOD_MS (OTA_AUTOFETCH_PERIOD * 60 * 60 * 1000)
#endif /* CONFIG_ESP_RMAKER_OTA_AUTOFETCH */

static const char *TAG = "esp_rmaker_ota_using_topics";

/* OTA fetch retry configuration */
#define OTA_FETCH_TIMEOUT_SECONDS   60
#define OTA_FETCH_RETRY_BASE_DELAY  30
#define OTA_FETCH_MAX_RETRIES       5



/* OTA fetch state management structure */
typedef struct {
    int expected_msg_id;
    unsigned int retry_count;
    bool fetch_in_progress;
    esp_event_handler_instance_t published_handler_instance;
    TimerHandle_t timeout_timer;  /* Timer for PUBACK timeout */
} ota_fetch_state_t;

static ota_fetch_state_t g_ota_fetch_state = {0};

/* Forward declarations */
static void ota_fetch_schedule_retry(void);
#ifdef CONFIG_ESP_RMAKER_OTA_AUTOFETCH
static void esp_rmaker_ota_autofetch_cleanup(void);
#endif

/* Helper functions for OTA fetch reliability */
static void ota_fetch_cleanup(void)
{
    if (g_ota_fetch_state.timeout_timer) {
        xTimerStop(g_ota_fetch_state.timeout_timer, portMAX_DELAY);
        xTimerDelete(g_ota_fetch_state.timeout_timer, portMAX_DELAY);
        g_ota_fetch_state.timeout_timer = NULL;
    }

    if (g_ota_fetch_state.published_handler_instance) {
        esp_event_handler_instance_unregister(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_PUBLISHED,
                                             g_ota_fetch_state.published_handler_instance);
        g_ota_fetch_state.published_handler_instance = NULL;
    }

    /* Reset state for next use instead of freeing */
    memset(&g_ota_fetch_state, 0, sizeof(g_ota_fetch_state));
}

static void ota_fetch_timeout_timer_cb(TimerHandle_t xTimer)
{
    /* Timeout case: No PUBACK received within timeout period */
    ESP_LOGW(TAG, "OTA fetch timeout - no PUBACK received");
    g_ota_fetch_state.fetch_in_progress = false;

    /* Clean up current attempt */
    if (g_ota_fetch_state.published_handler_instance) {
        esp_event_handler_instance_unregister(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_PUBLISHED,
                                             g_ota_fetch_state.published_handler_instance);
        g_ota_fetch_state.published_handler_instance = NULL;
    }

    /* Schedule retry using common retry logic */
    ota_fetch_schedule_retry();
}



static void ota_fetch_mqtt_event_handler(void *arg, esp_event_base_t event_base,
                                          int32_t event_id, void *event_data)
{
    if (!event_data || event_base != RMAKER_COMMON_EVENT) {
        return;
    }

    if (event_id == RMAKER_MQTT_EVENT_PUBLISHED) {
        int received_msg_id = *((int*)event_data);
        if (received_msg_id != g_ota_fetch_state.expected_msg_id) {
            return;
        }

        ota_fetch_cleanup();
    }
}

esp_err_t esp_rmaker_ota_report_status_using_topics(esp_rmaker_ota_handle_t ota_handle, ota_status_t status, char *additional_info)
{
    if (!ota_handle) {
        return ESP_FAIL;
    }
    esp_rmaker_ota_t *ota = (esp_rmaker_ota_t *)ota_handle;

    char publish_payload[200];
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, publish_payload, sizeof(publish_payload), NULL, NULL);
    json_gen_start_object(&jstr);
    if (ota->transient_priv) {
        json_gen_obj_set_string(&jstr, "ota_job_id", (char *)ota->transient_priv);
    } else {
        /* This will get executed only when the OTA status is being reported after a reboot, either to
         * indicate successful verification of new firmware, or to indicate that firmware was rolled back
         */
        nvs_handle handle;
        esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, RMAKER_OTA_NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (err == ESP_OK) {
            char job_id[64] = {0};
            size_t len = sizeof(job_id);
            if ((err = nvs_get_blob(handle, RMAKER_OTA_JOB_ID_NVS_NAME, job_id, &len)) == ESP_OK) {
                json_gen_obj_set_string(&jstr, "ota_job_id", job_id);
                nvs_erase_key(handle, RMAKER_OTA_JOB_ID_NVS_NAME);
            }
            nvs_close(handle);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Not reporting any status, since there is no Job ID available");
                return ESP_ERR_INVALID_STATE;
            }
        }
    }
    json_gen_obj_set_string(&jstr, "status", esp_rmaker_ota_status_to_string(status));
    char *network_id = esp_rmaker_get_network_id();
    if (network_id) {
        json_gen_obj_set_string(&jstr, "network_id", network_id);
    }
    json_gen_obj_set_string(&jstr, "additional_info", additional_info);
    /* Add timestamp field 'ts' */
    time_t current_time;
    time(&current_time);
    json_gen_obj_set_int(&jstr, "ts", (int)current_time);
    json_gen_end_object(&jstr);
    json_gen_str_end(&jstr);

    char publish_topic[MQTT_TOPIC_BUFFER_SIZE];
    esp_rmaker_create_mqtt_topic(publish_topic, sizeof(publish_topic), OTASTATUS_TOPIC_SUFFIX, OTASTATUS_TOPIC_RULE);
    ESP_LOGI(TAG, "%s",publish_payload);
    esp_err_t err = esp_rmaker_mqtt_publish(publish_topic, publish_payload, strlen(publish_payload),
                        RMAKER_MQTT_QOS1, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_rmaker_mqtt_publish_data returned error %d",err);
        return ESP_FAIL;
    }
    return ESP_OK;
}

void esp_rmaker_ota_finish_using_topics(esp_rmaker_ota_t *ota)
{
    if (ota->url) {
        free(ota->url);
        ota->url = NULL;
    }
#ifdef CONFIG_ESP_RMAKER_OTA_USE_MQTT
    if (ota->stream_id) {
        free(ota->stream_id);
        ota->stream_id = NULL;
    }
#endif
    ota->filesize = 0;
    if (ota->transient_priv) {
        free(ota->transient_priv);
        ota->transient_priv = NULL;
    }
    if (ota->metadata) {
        free(ota->metadata);
        ota->metadata = NULL;
    }
    if (ota->fw_version) {
        free(ota->fw_version);
        ota->fw_version = NULL;
    }
    if (ota->file_md5) {
        free(ota->file_md5);
        ota->file_md5 = NULL;
    }
    ota->ota_in_progress = false;
}
static void ota_url_handler(const char *topic, void *payload, size_t payload_len, void *priv_data)
{
    if (!priv_data) {
        return;
    }
    esp_rmaker_ota_handle_t ota_handle = priv_data;
    esp_rmaker_ota_t *ota = (esp_rmaker_ota_t *)ota_handle;
    if (ota->ota_in_progress) {
        ESP_LOGE(TAG, "OTA already in progress. Please try later.");
        return;
    }
    ota->ota_in_progress = true;
    /* Starting Firmware Upgrades */
    ESP_LOGI(TAG, "Upgrade Handler got:%.*s on %s topic\n", (int) payload_len, (char *)payload, topic);
    /*
       {
       "ota_job_id": "<ota_job_id>",
       "url": "<fw_url>",
       "file_md5": "<file_md5>",
       "fw_version": "<fw_version>",
       "filesize": <size_in_bytes>
       }
    */
    jparse_ctx_t jctx;
    char *url = NULL, *ota_job_id = NULL, *fw_version = NULL, *file_md5 = NULL;
#ifdef CONFIG_ESP_RMAKER_OTA_USE_MQTT
    char *stream_id = NULL;
#endif
    int ret = json_parse_start(&jctx, (char *)payload, (int) payload_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "Invalid JSON received: %s", (char *)payload);
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Aborted. JSON Payload error");
        ota->ota_in_progress = false;
        return;
    }
    int len = 0;
    ret = json_obj_get_strlen(&jctx, "ota_job_id", &len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Aborted. OTA Job ID not found in JSON");
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Aborted. OTA Updated ID not found in JSON");
        goto end;
    }
    len++; /* Increment for NULL character */
    ota_job_id = MEM_CALLOC_EXTRAM(1, len);
    if (!ota_job_id) {
        ESP_LOGE(TAG, "Aborted. OTA Job ID memory allocation failed");
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Aborted. OTA Updated ID memory allocation failed");
        goto end;
    }
    json_obj_get_string(&jctx, "ota_job_id", ota_job_id, len);
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, RMAKER_OTA_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_blob(handle, RMAKER_OTA_JOB_ID_NVS_NAME, ota_job_id, strlen(ota_job_id));
        nvs_close(handle);
    }
    ESP_LOGI(TAG, "OTA Job ID: %s", ota_job_id);
    ota->transient_priv = ota_job_id;
    len = 0;
    ret = json_obj_get_strlen(&jctx, "url", &len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Aborted. URL not found in JSON");
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Aborted. URL not found in JSON");
        goto end;
    }
    len++; /* Increment for NULL character */
    url = MEM_CALLOC_EXTRAM(1, len);
    if (!url) {
        ESP_LOGE(TAG, "Aborted. URL memory allocation failed");
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Aborted. URL memory allocation failed");
        goto end;
    }
    json_obj_get_string(&jctx, "url", url, len);
    ESP_LOGI(TAG, "URL: %s", url);
    len = 0;
    ret = json_obj_get_strlen(&jctx, "file_md5", &len);
    if (ret == ESP_OK) {
        len++; /* Increment for NULL character */
        file_md5 = MEM_CALLOC_EXTRAM(1, len);
        if (!file_md5) {
            ESP_LOGE(TAG, "Aborted. File MD5 memory allocation failed");
            esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Aborted. File MD5 memory allocation failed");
            goto end;
        }
        json_obj_get_string(&jctx, "file_md5", file_md5, len);
        ESP_LOGI(TAG, "File MD5: %s", file_md5);
    }
#ifdef CONFIG_ESP_RMAKER_OTA_USE_MQTT
    len = 0;
    ret = json_obj_get_strlen(&jctx, "stream_id", &len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Aborted. Stream ID not found in JSON");
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Aborted. Stream ID not found in JSON");
        goto end;
    }
    len++; /* Increment for NULL character */
    stream_id = calloc(1, len);
    if (!stream_id) {
        ESP_LOGE(TAG, "Aborted. Stream ID memory allocation failed");
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Aborted. Stream ID memory allocation failed");
        goto end;
    }
    json_obj_get_string(&jctx, "stream_id", stream_id, len);
    ESP_LOGI(TAG, "Stream ID: %s", stream_id);
#endif
    int filesize = 0;
    json_obj_get_int(&jctx, "file_size", &filesize);
    ESP_LOGI(TAG, "File Size: %d", filesize);
    len = 0;
    ret = json_obj_get_strlen(&jctx, "fw_version", &len);
    if (ret == ESP_OK && len > 0) {
        len++; /* Increment for NULL character */
        fw_version = MEM_CALLOC_EXTRAM(1, len);
        if (!fw_version) {
            ESP_LOGE(TAG, "Aborted. Firmware version memory allocation failed");
            esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Aborted. Firmware version memory allocation failed");
            goto end;
        }
        json_obj_get_string(&jctx, "fw_version", fw_version, len);
        ESP_LOGI(TAG, "Firmware version: %s", fw_version);
    }

    int metadata_size = 0;
    char *metadata = NULL;
    ret = json_obj_get_object_strlen(&jctx, "metadata", &metadata_size);
    if (ret == ESP_OK && metadata_size > 0) {
        metadata_size++; /* Increment for NULL character */
        metadata = MEM_CALLOC_EXTRAM(1, metadata_size);
        if (!metadata) {
            ESP_LOGE(TAG, "Aborted. OTA metadata memory allocation failed");
            esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Aborted. OTA metadata memory allocation failed");
            goto end;
        }
        json_obj_get_object_str(&jctx, "metadata", metadata, metadata_size);
        ota->metadata = metadata;
    }

    json_parse_end(&jctx);
    if (ota->url) {
        free(ota->url);
    }
    ota->url = url;
#ifdef CONFIG_ESP_RMAKER_OTA_USE_MQTT
    ota->stream_id = stream_id;
#endif
    ota->fw_version = fw_version;
    ota->file_md5 = file_md5;
    ota->filesize = filesize;
    ota->ota_in_progress = true;
    if (esp_rmaker_work_queue_add_task(esp_rmaker_ota_common_cb, ota) != ESP_OK) {
        esp_rmaker_ota_finish_using_topics(ota);
    }
    return;
end:
    if (url) {
        free(url);
    }
#ifdef CONFIG_ESP_RMAKER_OTA_USE_MQTT
    if (stream_id) {
        free(stream_id);
    }
#endif
    if (fw_version) {
        free(fw_version);
    }
    if (file_md5) {
        free(file_md5);
    }
    esp_rmaker_ota_finish_using_topics(ota);
    json_parse_end(&jctx);
    return;
}

static esp_err_t __esp_rmaker_ota_fetch(void)
{
    /* Check if fetch already in progress */
    if (g_ota_fetch_state.fetch_in_progress) {
        ESP_LOGW(TAG, "OTA fetch already in progress. Skipping.");
        /* Returning success since OTA fetch is already in progress */
        return ESP_OK;
    }

    /* Verify MQTT connection */
    if (!esp_rmaker_is_mqtt_connected()) {
        ESP_LOGW(TAG, "MQTT not connected. Cannot fetch OTA.");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Fetching OTA details, if any.");
    esp_rmaker_node_info_t *info = esp_rmaker_node_get_info(esp_rmaker_get_node());
    if (!info) {
        ESP_LOGE(TAG, "Node info not found. Cant send otafetch request");
        return ESP_FAIL;
    }

    /* Initialize state for this fetch */
    g_ota_fetch_state.expected_msg_id = -1;
    g_ota_fetch_state.retry_count = 1;

    char publish_payload[150];
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, publish_payload, sizeof(publish_payload), NULL, NULL);
    json_gen_start_object(&jstr);
    json_gen_obj_set_string(&jstr, "node_id", esp_rmaker_get_node_id());
    json_gen_obj_set_string(&jstr, "fw_version", info->fw_version);
    char *network_id = esp_rmaker_get_network_id();
    if (network_id) {
        json_gen_obj_set_string(&jstr, "network_id", network_id);
    }
    json_gen_end_object(&jstr);
    json_gen_str_end(&jstr);
    char publish_topic[MQTT_TOPIC_BUFFER_SIZE];
    esp_rmaker_create_mqtt_topic(publish_topic, sizeof(publish_topic), OTAFETCH_TOPIC_SUFFIX, OTAFETCH_TOPIC_RULE);

    int msg_id = -1;
    esp_err_t err = esp_rmaker_mqtt_publish(publish_topic, publish_payload, strlen(publish_payload),
                        RMAKER_MQTT_QOS1, &msg_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA Fetch Publish Error %d", err);
        return err;
    }

    if (msg_id == -1) {
        ESP_LOGE(TAG, "Failed to get message ID for OTA fetch");
        return ESP_FAIL;
    }

    /* Track the message for enhanced delivery */
    g_ota_fetch_state.expected_msg_id = msg_id;
    g_ota_fetch_state.fetch_in_progress = true;

    /* Register event handler for MQTT PUBLISHED event */
    err = esp_event_handler_instance_register(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_PUBLISHED,
                                            ota_fetch_mqtt_event_handler, NULL,
                                            &g_ota_fetch_state.published_handler_instance);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT PUBLISHED event handler: %s", esp_err_to_name(err));
        ota_fetch_cleanup();
        return err;
    }

    /* Create timeout timer for PUBACK confirmation */
    g_ota_fetch_state.timeout_timer = xTimerCreate("ota_timeout",
                                                   pdMS_TO_TICKS(OTA_FETCH_TIMEOUT_SECONDS * 1000),
                                                   pdFALSE, NULL, ota_fetch_timeout_timer_cb);
    if (!g_ota_fetch_state.timeout_timer) {
        ESP_LOGE(TAG, "Failed to create timeout timer");
        ota_fetch_cleanup();
        return ESP_ERR_NO_MEM;
    }

    if (xTimerStart(g_ota_fetch_state.timeout_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timeout timer");
        ota_fetch_cleanup();
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void ota_fetch_schedule_retry(void)
{
    g_ota_fetch_state.retry_count++;

    /* Calculate delay with exponential backoff, but cap at maximum */
    int delay;
    if (g_ota_fetch_state.retry_count <= OTA_FETCH_MAX_RETRIES) {
        delay = OTA_FETCH_RETRY_BASE_DELAY * (1 << (g_ota_fetch_state.retry_count - 1));
        ESP_LOGI(TAG, "OTA fetch failed, retry %u in %d seconds",
                 g_ota_fetch_state.retry_count, delay);
    } else {
        /* Continue retrying at maximum interval indefinitely */
        delay = OTA_FETCH_RETRY_BASE_DELAY * (1 << (OTA_FETCH_MAX_RETRIES - 1));
        ESP_LOGI(TAG, "OTA fetch failed, persistent retry %u at max interval (%d seconds)",
                 g_ota_fetch_state.retry_count, delay);
    }

    /* Use existing delay function for retry scheduling */
    esp_err_t err = esp_rmaker_ota_fetch_with_delay(delay);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to schedule OTA retry: %s", esp_err_to_name(err));
        ota_fetch_cleanup();
    }
}

esp_err_t esp_rmaker_ota_fetch(void)
{
    esp_err_t err = __esp_rmaker_ota_fetch();

    /* If the fetch attempt failed (including publish failures), schedule retry */
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "OTA fetch attempt failed: %s", esp_err_to_name(err));

        /* Initialize state for retry tracking */
        g_ota_fetch_state.expected_msg_id = -1;
        g_ota_fetch_state.retry_count = 0; /* Start from 0 since this is the first failure */
        g_ota_fetch_state.fetch_in_progress = false;

        /* Schedule retry with exponential backoff */
        ota_fetch_schedule_retry();
    }

    return err;
}

#ifdef CONFIG_ESP_RMAKER_OTA_AUTOFETCH
static void esp_rmaker_ota_autofetch_timer_cb(TimerHandle_t xTimer)
{
    esp_rmaker_ota_fetch();
    /* Restart for next autofetch period */
    xTimerStart(xTimer, 0);
}
static void esp_rmaker_ota_autofetch_cleanup(void)
{
    if (ota_autofetch_timer) {
        xTimerStop(ota_autofetch_timer, portMAX_DELAY);
        xTimerDelete(ota_autofetch_timer, portMAX_DELAY);
        ota_autofetch_timer = NULL;
    }
}
#endif

static esp_err_t esp_rmaker_ota_subscribe(void *priv_data)
{
    char subscribe_topic[MQTT_TOPIC_BUFFER_SIZE];

    snprintf(subscribe_topic, sizeof(subscribe_topic),"node/%s/%s", esp_rmaker_get_node_id(), OTAURL_TOPIC_SUFFIX);

    ESP_LOGI(TAG, "Subscribing to: %s", subscribe_topic);
    /* First unsubscribe, in case there is a stale subscription */
    esp_rmaker_mqtt_unsubscribe(subscribe_topic);
    esp_err_t err = esp_rmaker_mqtt_subscribe(subscribe_topic, ota_url_handler, RMAKER_MQTT_QOS1, priv_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA URL Subscription Error %d", err);
    }
    return err;
}

static void esp_rmaker_ota_work_fn(void *priv_data)
{
    esp_rmaker_ota_t *ota = (esp_rmaker_ota_t *)priv_data;
    /* If the firmware was rolled back, indicate that first */
    if (ota->rolled_back) {
        esp_rmaker_ota_report_status((esp_rmaker_ota_handle_t )ota, OTA_STATUS_REJECTED, "Firmware rolled back");
        ota->rolled_back = false;
    }
    esp_rmaker_ota_subscribe(priv_data);
#ifdef CONFIG_ESP_RMAKER_OTA_AUTOFETCH
    if (ota->ota_in_progress != true) {
        if (esp_rmaker_ota_fetch_with_delay(RMAKER_OTA_FETCH_DELAY) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create OTA Fetch timer.");
        }
    }
    if (OTA_AUTOFETCH_PERIOD > 0) {
        /* Clean up any existing timer first */
        esp_rmaker_ota_autofetch_cleanup();

        ota_autofetch_timer = xTimerCreate("ota_autofetch_tm",
                                         pdMS_TO_TICKS(OTA_AUTOFETCH_PERIOD_MS),
                                         pdFALSE, NULL, esp_rmaker_ota_autofetch_timer_cb);
        if (ota_autofetch_timer == NULL) {
            ESP_LOGE(TAG, "Failed to create OTA Autofetch timer");
        } else {
            xTimerStart(ota_autofetch_timer, 0);
        }
    }
#endif /* CONFIG_ESP_RMAKER_OTA_AUTOFETCH */
}

/* Enable the ESP RainMaker specific OTA */
esp_err_t esp_rmaker_ota_enable_using_topics(esp_rmaker_ota_t *ota)
{
    esp_err_t err = esp_rmaker_work_queue_add_task(esp_rmaker_ota_work_fn, ota);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA enabled with Topics");
    }
    return err;
}

static void esp_rmaker_ota_fetch_timer_cb(TimerHandle_t xTimer)
{
    esp_rmaker_ota_fetch();
    xTimerDelete(xTimer, 0);
}

esp_err_t esp_rmaker_ota_fetch_with_delay(int time)
{
    TimerHandle_t timer = xTimerCreate(NULL, (time * 1000) / portTICK_PERIOD_MS, pdFALSE, NULL, esp_rmaker_ota_fetch_timer_cb);
    if (timer == NULL) {
        return ESP_ERR_NO_MEM;
    } else {
        xTimerStart(timer, 0);
    }
    return ESP_OK;
}
