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

#include <string.h>
#include <esp_log.h>
#include <esp_event.h>
#include <nvs.h>
#include <wifi_provisioning/manager.h>
#include <json_generator.h>
#include <esp_rmaker_work_queue.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_user_mapping.h>
#include <esp_rmaker_mqtt.h>
#include <esp_rmaker_common_events.h>
#include "esp_rmaker_user_mapping.pb-c.h"
#include "esp_rmaker_internal.h"

static const char *TAG = "esp_rmaker_user_mapping";

#define USER_MAPPING_TOPIC_SUFFIX   "user/mapping"
#define USER_MAPPING_ENDPOINT       "cloud_user_assoc"
#define USER_MAPPING_NVS_NAMESPACE  "user_mapping"
#define USER_ID_NVS_NAME            "user_id"

typedef struct {
    char *user_id;
    char *secret_key;
    int mqtt_msg_id;
} esp_rmaker_user_mapping_data_t;

static esp_rmaker_user_mapping_data_t *rmaker_user_mapping_data;
esp_rmaker_user_mapping_state_t rmaker_user_mapping_state;

static void esp_rmaker_user_mapping_cleanup_data(void)
{
    if (rmaker_user_mapping_data) {
        if (rmaker_user_mapping_data->user_id) {
            free(rmaker_user_mapping_data->user_id);
        }
        if (rmaker_user_mapping_data->secret_key) {
            free(rmaker_user_mapping_data->secret_key);
        }
        free(rmaker_user_mapping_data);
        rmaker_user_mapping_data = NULL;
    }
}

static void esp_rmaker_user_mapping_event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_INIT: {
                if (esp_rmaker_user_mapping_endpoint_create() != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to create user mapping end point.");
                }
                break;
            }
            case WIFI_PROV_START:
                if (esp_rmaker_user_mapping_endpoint_register() != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to register user mapping end point.");
                }
                break;
            default:
                break;
        }
    } else if ((event_base == RMAKER_COMMON_EVENT) && (event_id == RMAKER_MQTT_EVENT_PUBLISHED)) {
        /* Checking for the PUBACK for the user node association message to be sure that the message
         * has indeed reached the RainMaker cloud.
         */
        int msg_id = *((int *)event_data);
        if (msg_id == rmaker_user_mapping_data->mqtt_msg_id) {
            ESP_LOGI(TAG, "User Node association message published successfully.");
            rmaker_user_mapping_state = ESP_RMAKER_USER_MAPPING_DONE;
#ifdef CONFIG_ESP_RMAKER_USER_ID_CHECK
            /* Store User Id in NVS since acknowledgement of the user-node association message is received */
            nvs_handle handle;
            esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, USER_MAPPING_NVS_NAMESPACE, NVS_READWRITE, &handle);
            if (err == ESP_OK) {
                nvs_set_blob(handle, USER_ID_NVS_NAME, rmaker_user_mapping_data->user_id, strlen(rmaker_user_mapping_data->user_id));
                nvs_close(handle);
            }
#endif
            esp_rmaker_post_event(RMAKER_EVENT_USER_NODE_MAPPING_DONE, rmaker_user_mapping_data->user_id,
                    strlen(rmaker_user_mapping_data->user_id) + 1);
            esp_rmaker_user_mapping_cleanup_data();
            esp_event_handler_unregister(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_PUBLISHED,
                    &esp_rmaker_user_mapping_event_handler);
        }
    }
}

static void esp_rmaker_user_mapping_cb(void *priv_data)
{
    esp_rmaker_user_mapping_data_t *data = (esp_rmaker_user_mapping_data_t *)priv_data;
    if (!data) {
        return;
    }
    esp_event_handler_register(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_PUBLISHED,
            &esp_rmaker_user_mapping_event_handler, NULL);
    char publish_payload[200];
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, publish_payload, sizeof(publish_payload), NULL, NULL);
    json_gen_start_object(&jstr);
    char *node_id = esp_rmaker_get_node_id();
    json_gen_obj_set_string(&jstr, "node_id", node_id);
    json_gen_obj_set_string(&jstr, "user_id", data->user_id);
    json_gen_obj_set_string(&jstr, "secret_key", data->secret_key);
    if (esp_rmaker_user_node_mapping_get_state() != ESP_RMAKER_USER_MAPPING_DONE) {
        json_gen_obj_set_bool(&jstr, "reset", true);
    }
    json_gen_end_object(&jstr);
    json_gen_str_end(&jstr);
    char publish_topic[100];
    snprintf(publish_topic, sizeof(publish_topic), "node/%s/%s", node_id, USER_MAPPING_TOPIC_SUFFIX);
    esp_err_t err = esp_rmaker_mqtt_publish(publish_topic, publish_payload, strlen(publish_payload), RMAKER_MQTT_QOS1, &data->mqtt_msg_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MQTT Publish Error %d", err);
    }
    return;
}

static bool esp_rmaker_user_mapping_detect_reset(const char *user_id)
{
#ifdef CONFIG_ESP_RMAKER_USER_ID_CHECK
    bool reset_state = true;
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, USER_MAPPING_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return true;
    }
    char *nvs_user_id = NULL;
    size_t len = 0;
    if ((err = nvs_get_blob(handle, USER_ID_NVS_NAME, NULL, &len)) == ESP_OK) {
        nvs_user_id = calloc(1, len + 1); /* +1 for NULL termination */
        if (nvs_user_id) {
            nvs_get_blob(handle, USER_ID_NVS_NAME, nvs_user_id, &len);
            /* If existing user id and new user id are same, this is not a reset state */
            if (strcmp(nvs_user_id, user_id) == 0) {
                reset_state = false;
            } else {
                /* Deleting the key in case of a mismatch. It will be stored only after the user node association
                 * message is acknowledged from the cloud.
                 */
                nvs_erase_key(handle, USER_ID_NVS_NAME);
            }
            free(nvs_user_id);
        }
    }
    nvs_close(handle);
    return reset_state;
#else
    return false;
#endif
}

esp_err_t esp_rmaker_start_user_node_mapping(char *user_id, char *secret_key)
{
    if (rmaker_user_mapping_data) {
        esp_rmaker_user_mapping_cleanup_data();
    }

    rmaker_user_mapping_data = calloc(1, sizeof(esp_rmaker_user_mapping_data_t));
    if (!rmaker_user_mapping_data) {
        return ESP_FAIL;
    }
    rmaker_user_mapping_data->user_id = strdup(user_id);
    if (!rmaker_user_mapping_data->user_id) {
        goto user_mapping_error;
    }
    rmaker_user_mapping_data->secret_key = strdup(secret_key);
    if (!rmaker_user_mapping_data->secret_key) {
        goto user_mapping_error;
    }
    if (esp_rmaker_user_mapping_detect_reset(user_id)) {
        ESP_LOGI(TAG, "User Node mapping reset detected.");
        rmaker_user_mapping_state = ESP_RMAKER_USER_MAPPING_STARTED;
    } else {
        rmaker_user_mapping_state = ESP_RMAKER_USER_MAPPING_DONE;
    }
    if (esp_rmaker_work_queue_add_task(esp_rmaker_user_mapping_cb, rmaker_user_mapping_data) != ESP_OK) {
        goto user_mapping_error;
    }
    esp_rmaker_user_mapping_prov_deinit();
    return ESP_OK;

user_mapping_error:
    esp_rmaker_user_mapping_cleanup_data();
    return ESP_FAIL;
}

int esp_rmaker_user_mapping_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen, uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    Rainmaker__RMakerConfigPayload *data;
    data = rainmaker__rmaker_config_payload__unpack(NULL, inlen, inbuf);

    if (!data) {
        ESP_LOGE(TAG, "No Data Received");
        return ESP_FAIL;
    }
    switch (data->msg) {
	case RAINMAKER__RMAKER_CONFIG_MSG_TYPE__TypeCmdSetUserMapping: {
	    ESP_LOGI(TAG, "Received request for node details");
	    Rainmaker__RMakerConfigPayload resp;
	    Rainmaker__RespSetUserMapping payload;
	    rainmaker__rmaker_config_payload__init(&resp);
	    rainmaker__resp_set_user_mapping__init(&payload);

        if (data->payload_case != RAINMAKER__RMAKER_CONFIG_PAYLOAD__PAYLOAD_CMD_SET_USER_MAPPING) {
            ESP_LOGE(TAG, "Invalid payload type in the message: %d", data->payload_case);
            payload.status = RAINMAKER__RMAKER_CONFIG_STATUS__InvalidParam;
        } else {
            ESP_LOGI(TAG, "Got user_id = %s, secret_key = %s", data->cmd_set_user_mapping->userid, data->cmd_set_user_mapping->secretkey);
            if (esp_rmaker_start_user_node_mapping(data->cmd_set_user_mapping->userid,
                        data->cmd_set_user_mapping->secretkey) != ESP_OK) {
                ESP_LOGI(TAG, "Sending status Invalid Param");
                payload.status = RAINMAKER__RMAKER_CONFIG_STATUS__InvalidParam;
            } else {
                ESP_LOGI(TAG, "Sending status SUCCESS");
                payload.status = RAINMAKER__RMAKER_CONFIG_STATUS__Success;
                payload.nodeid = esp_rmaker_get_node_id();
            }
        }
        resp.msg = RAINMAKER__RMAKER_CONFIG_MSG_TYPE__TypeRespSetUserMapping;
        resp.payload_case = RAINMAKER__RMAKER_CONFIG_PAYLOAD__PAYLOAD_RESP_SET_USER_MAPPING;
	    resp.resp_set_user_mapping = &payload;

	    *outlen = rainmaker__rmaker_config_payload__get_packed_size(&resp);
	    *outbuf = (uint8_t *)malloc(*outlen);
	    rainmaker__rmaker_config_payload__pack(&resp, *outbuf);
        break;
    }
    default:
	    ESP_LOGE(TAG, "Received invalid message type: %d", data->msg);
	    break;
    }
    rainmaker__rmaker_config_payload__free_unpacked(data, NULL);
    return ESP_OK;
}
esp_err_t esp_rmaker_user_mapping_endpoint_create(void)
{
    esp_err_t err = wifi_prov_mgr_endpoint_create(USER_MAPPING_ENDPOINT);
    return err;
}

esp_err_t esp_rmaker_user_mapping_endpoint_register(void)
{
    return wifi_prov_mgr_endpoint_register(USER_MAPPING_ENDPOINT, esp_rmaker_user_mapping_handler, NULL);
}

esp_err_t esp_rmaker_user_mapping_prov_init(void)
{
    int ret = ESP_OK;
    ret = esp_event_handler_register(WIFI_PROV_EVENT, WIFI_PROV_INIT, &esp_rmaker_user_mapping_event_handler, NULL);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_event_handler_register(WIFI_PROV_EVENT, WIFI_PROV_START, &esp_rmaker_user_mapping_event_handler, NULL);
    return ret;
}

esp_err_t esp_rmaker_user_mapping_prov_deinit(void)
{
    esp_event_handler_unregister(WIFI_PROV_EVENT, WIFI_PROV_INIT, &esp_rmaker_user_mapping_event_handler);
    esp_event_handler_unregister(WIFI_PROV_EVENT, WIFI_PROV_START, &esp_rmaker_user_mapping_event_handler);
    return ESP_OK;
}

esp_rmaker_user_mapping_state_t esp_rmaker_user_node_mapping_get_state(void)
{
    return rmaker_user_mapping_state;
}

esp_err_t esp_rmaker_user_node_mapping_init(void)
{
#ifdef CONFIG_ESP_RMAKER_USER_ID_CHECK
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, USER_MAPPING_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        size_t len = 0;
        if ((err = nvs_get_blob(handle, USER_ID_NVS_NAME, NULL, &len)) == ESP_OK) {
            /* Some User Id found, which means that user node association is already done */
            rmaker_user_mapping_state = ESP_RMAKER_USER_MAPPING_DONE;
        }
        nvs_close(handle);
    }
#else
    rmaker_user_mapping_state = ESP_RMAKER_USER_MAPPING_DONE;
#endif
    return ESP_OK;
}
