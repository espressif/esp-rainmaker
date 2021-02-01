// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <stdint.h>
#include <string.h>
#include <json_parser.h>
#include <json_generator.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_rmaker_work_queue.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_ota.h>

#include "esp_rmaker_ota_internal.h"
#include "esp_rmaker_mqtt.h"

#ifdef CONFIG_ESP_RMAKER_OTA_AUTOFETCH
#include <esp_timer.h>
static esp_timer_handle_t ota_autofetch_timer;
/* Autofetch period in hours */
#define OTA_AUTOFETCH_PERIOD   CONFIG_ESP_RMAKER_OTA_AUTOFETCH_PERIOD 
/* Autofetch period in micro-seconds */
static uint64_t ota_autofetch_period = (OTA_AUTOFETCH_PERIOD * 60 * 60 * 1000000LL);
#endif /* CONFIG_ESP_RMAKER_OTA_AUTOFETCH */

static const char *TAG = "esp_rmaker_ota_using_topics";

#define OTAURL_TOPIC_SUFFIX     "otaurl"
#define OTAFETCH_TOPIC_SUFFIX   "otafetch"
#define OTASTATUS_TOPIC_SUFFIX  "otastatus"

esp_err_t esp_rmaker_ota_report_status_using_topics(esp_rmaker_ota_handle_t ota_handle, ota_status_t status, char *additional_info)
{
    if (!ota_handle) {
        return ESP_FAIL;
    }
    esp_rmaker_ota_t *ota = (esp_rmaker_ota_t *)ota_handle;

    char publish_payload[200];
    char *node_id = esp_rmaker_get_node_id();
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, publish_payload, sizeof(publish_payload), NULL, NULL);
    json_gen_start_object(&jstr);
    if (ota->transient_priv) {
        json_gen_obj_set_string(&jstr, "ota_job_id", (char *)ota->transient_priv);
    }
    json_gen_obj_set_string(&jstr, "status", esp_rmaker_ota_status_to_string(status));
    json_gen_obj_set_string(&jstr, "additional_info", additional_info);
    json_gen_end_object(&jstr);
    json_gen_str_end(&jstr);

    char publish_topic[100];
    snprintf(publish_topic, sizeof(publish_topic), "node/%s/%s", node_id, OTASTATUS_TOPIC_SUFFIX);
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
    ota->filesize = 0;
    if (ota->transient_priv) {
        free(ota->transient_priv);
        ota->transient_priv = NULL;
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
       "fw_version": "<fw_version>",
       "filesize": <size_in_bytes>
       }
    */
    jparse_ctx_t jctx;
    char *url = NULL, *ota_job_id = NULL;
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
        ESP_LOGE(TAG, "Aborted. OTA Updated ID not found in JSON");
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Aborted. OTA Updated ID not found in JSON");
        goto end;
    }
    len++; /* Increment for NULL character */
    ota_job_id = calloc(1, len);
    if (!ota_job_id) {
        ESP_LOGE(TAG, "Aborted. OTA Updated ID memory allocation failed");
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Aborted. OTA Updated ID memory allocation failed");
        goto end;
    }
    json_obj_get_string(&jctx, "ota_job_id", ota_job_id, len);
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
    url = calloc(1, len);
    if (!url) {
        ESP_LOGE(TAG, "Aborted. URL memory allocation failed");
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Aborted. URL memory allocation failed");
        goto end;
    }
    json_obj_get_string(&jctx, "url", url, len);
    ESP_LOGI(TAG, "URL: %s", url);

    int filesize = 0;
    json_obj_get_int(&jctx, "file_size", &filesize);
    ESP_LOGI(TAG, "File Size: %d", filesize);

    json_parse_end(&jctx);
    if (ota->url) {
        free(ota->url);
    }
    ota->url = url;
    ota->filesize = filesize;
    ota->ota_in_progress = true;
    if (esp_rmaker_work_queue_add_task(esp_rmaker_ota_common_cb, ota) != ESP_OK) {
        esp_rmaker_ota_finish_using_topics(ota);
    }
    return;
end:
    esp_rmaker_ota_finish_using_topics(ota);
    json_parse_end(&jctx);
    return;
}

#ifdef CONFIG_ESP_RMAKER_OTA_AUTOFETCH
static void esp_rmaker_ota_fetch(void *priv)
{
    esp_rmaker_node_info_t *info = esp_rmaker_node_get_info(esp_rmaker_get_node());
    if (!info) {
        ESP_LOGE(TAG, "Node info not found. Cant send otafetch request");
        return;
    }
    char publish_payload[150];
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, publish_payload, sizeof(publish_payload), NULL, NULL);
    json_gen_start_object(&jstr);
    json_gen_obj_set_string(&jstr, "node_id", esp_rmaker_get_node_id());
    json_gen_obj_set_string(&jstr, "fw_version", info->fw_version);
    json_gen_end_object(&jstr);
    json_gen_str_end(&jstr);
    char publish_topic[100];
    snprintf(publish_topic, sizeof(publish_topic), "node/%s/%s", esp_rmaker_get_node_id(), OTAFETCH_TOPIC_SUFFIX);
    esp_err_t err = esp_rmaker_mqtt_publish(publish_topic, publish_payload, strlen(publish_payload),
                        RMAKER_MQTT_QOS1, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA Fetch Publish Error %d", err);
    }
}
#endif /* CONFIG_ESP_RMAKER_OTA_AUTOFETCH */

static esp_err_t esp_rmaker_ota_subscribe(void *priv_data)
{
    char subscribe_topic[100];

    snprintf(subscribe_topic, sizeof(subscribe_topic),"node/%s/%s", esp_rmaker_get_node_id(), OTAURL_TOPIC_SUFFIX);

    ESP_LOGI(TAG, "Subscribing to: %s", subscribe_topic);
    /* First unsubscribe, in case there is a stale subscription */
    esp_rmaker_mqtt_unsubscribe(subscribe_topic);
    esp_err_t err = esp_rmaker_mqtt_subscribe(subscribe_topic, ota_url_handler, RMAKER_MQTT_QOS1, priv_data);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "OTA URL Subscription Error %d", err);
    }
    return err;
}

static void esp_rmaker_ota_work_fn(void *priv_data)
{
    esp_rmaker_ota_subscribe(priv_data);
#ifdef CONFIG_ESP_RMAKER_OTA_AUTOFETCH
    esp_rmaker_ota_fetch(priv_data);
    if (ota_autofetch_period > 0) {
        esp_timer_create_args_t autofetch_timer_conf = {
            .callback = esp_rmaker_ota_fetch,
            .arg = priv_data,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "ota_autofetch_tm"
        };
        if (esp_timer_create(&autofetch_timer_conf, &ota_autofetch_timer) == ESP_OK) {
            esp_timer_start_periodic(ota_autofetch_timer, ota_autofetch_period);
        } else {
            ESP_LOGE(TAG, "Failes to create OTA Autofetch timer");
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
