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
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include <json_generator.h>
#include <esp_rmaker_work_queue.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_user_mapping.h>
#include <esp_rmaker_mqtt.h>
#include <esp_rmaker_utils.h>
#include <esp_rmaker_common_events.h>
#include "esp_rmaker_user_mapping.pb-c.h"
#include "esp_rmaker_internal.h"
#include "esp_rmaker_mqtt_topics.h"
#include <network_provisioning/manager.h>

static const char *TAG = "esp_rmaker_user_mapping";

#define USER_MAPPING_ENDPOINT       "cloud_user_assoc"
#define USER_MAPPING_NVS_NAMESPACE  "user_mapping"
#define USER_ID_NVS_NAME            "user_id"
#define USER_RESET_ID               "esp-rmaker"
#define USER_RESET_KEY              "failed"

/* A delay large enough to allow the tasks to get the semaphore, but small
 * enough to prevent tasks getting blocked for long.
 */
#define SEMAPHORE_DELAY_MSEC         5000

/* User mapping queue retry configuration */
#define USER_MAPPING_RETRY_DELAY_SECONDS  5
#define USER_MAPPING_MAX_RETRIES          5

typedef struct {
    char *user_id;
    char *secret_key;
    int mqtt_msg_id;
    bool sent;
} esp_rmaker_user_mapping_data_t;

/* User mapping queue retry state management structure */
typedef struct {
    unsigned int retry_count;
    bool retry_in_progress;
    TimerHandle_t retry_timer;
} user_mapping_retry_state_t;

static esp_rmaker_user_mapping_data_t *rmaker_user_mapping_data;
esp_rmaker_user_mapping_state_t rmaker_user_mapping_state;
SemaphoreHandle_t esp_rmaker_user_mapping_lock = NULL;
static user_mapping_retry_state_t g_user_mapping_retry_state = {0};

/* Forward declarations */
static void esp_rmaker_user_mapping_cb(void *priv_data);
static void user_mapping_retry_cleanup(void);
static void user_mapping_schedule_retry(void);

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

/* Helper functions for user mapping queue retry */
static void user_mapping_retry_cleanup(void)
{
    if (g_user_mapping_retry_state.retry_timer) {
        xTimerStop(g_user_mapping_retry_state.retry_timer, portMAX_DELAY);
        xTimerDelete(g_user_mapping_retry_state.retry_timer, portMAX_DELAY);
        g_user_mapping_retry_state.retry_timer = NULL;
    }

    /* Reset state for next use */
    memset(&g_user_mapping_retry_state, 0, sizeof(g_user_mapping_retry_state));
}

static void user_mapping_retry_timer_cb(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Retrying user mapping task queue (attempt %u/%u)",
             g_user_mapping_retry_state.retry_count, USER_MAPPING_MAX_RETRIES);

    g_user_mapping_retry_state.retry_in_progress = false;

    /* Try to queue the user mapping task again */
    if (esp_rmaker_work_queue_add_task(esp_rmaker_user_mapping_cb, NULL) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue user mapping task on retry %u", g_user_mapping_retry_state.retry_count);
        /* Schedule another retry if we haven't exceeded max attempts */
        user_mapping_schedule_retry();
    } else {
        ESP_LOGI(TAG, "Successfully queued user mapping task on retry %u", g_user_mapping_retry_state.retry_count);
        user_mapping_retry_cleanup();
    }
}

static void user_mapping_schedule_retry(void)
{
    g_user_mapping_retry_state.retry_count++;

    if (g_user_mapping_retry_state.retry_count > USER_MAPPING_MAX_RETRIES) {
        ESP_LOGE(TAG, "User mapping task queue failed after %u attempts. Giving up.", USER_MAPPING_MAX_RETRIES);
        user_mapping_retry_cleanup();
        return;
    }

    if (g_user_mapping_retry_state.retry_in_progress) {
        ESP_LOGW(TAG, "User mapping retry already in progress. Skipping.");
        return;
    }

    g_user_mapping_retry_state.retry_in_progress = true;

    ESP_LOGI(TAG, "Scheduling user mapping retry %u/%u in %d seconds",
             g_user_mapping_retry_state.retry_count, USER_MAPPING_MAX_RETRIES, USER_MAPPING_RETRY_DELAY_SECONDS);

    /* Check if timer already exists and clean it up to prevent resource leak */
    if (g_user_mapping_retry_state.retry_timer) {
        xTimerStop(g_user_mapping_retry_state.retry_timer, portMAX_DELAY);
        xTimerDelete(g_user_mapping_retry_state.retry_timer, portMAX_DELAY);
        g_user_mapping_retry_state.retry_timer = NULL;
    }

    /* Create retry timer */
    g_user_mapping_retry_state.retry_timer = xTimerCreate("user_mapping_retry",
                                                          pdMS_TO_TICKS(USER_MAPPING_RETRY_DELAY_SECONDS * 1000),
                                                          pdFALSE, NULL, user_mapping_retry_timer_cb);
    if (!g_user_mapping_retry_state.retry_timer) {
        ESP_LOGE(TAG, "Failed to create user mapping retry timer");
        user_mapping_retry_cleanup();
        return;
    }

    if (xTimerStart(g_user_mapping_retry_state.retry_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start user mapping retry timer");
        user_mapping_retry_cleanup();
    }
}

static void esp_rmaker_user_mapping_event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == NETWORK_PROV_EVENT) {
        switch (event_id) {
            case NETWORK_PROV_INIT: {
                if (esp_rmaker_user_mapping_endpoint_create() != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to create user mapping end point.");
                }
                break;
            }
            case NETWORK_PROV_START:
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
        if (xSemaphoreTake(esp_rmaker_user_mapping_lock, SEMAPHORE_DELAY_MSEC/portTICK_PERIOD_MS) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to take semaphore.");
            return;
        }
        if ((rmaker_user_mapping_data != NULL) && (msg_id == rmaker_user_mapping_data->mqtt_msg_id)) {
            ESP_LOGI(TAG, "User Node association message published successfully.");
            if (strcmp(rmaker_user_mapping_data->user_id, USER_RESET_ID) == 0) {
                rmaker_user_mapping_state = ESP_RMAKER_USER_MAPPING_RESET;
                esp_rmaker_post_event(RMAKER_EVENT_USER_NODE_MAPPING_RESET, NULL, 0);
            } else {
                rmaker_user_mapping_state = ESP_RMAKER_USER_MAPPING_DONE;
                esp_rmaker_post_event(RMAKER_EVENT_USER_NODE_MAPPING_DONE, rmaker_user_mapping_data->user_id,
                    strlen(rmaker_user_mapping_data->user_id) + 1);
            }
#ifdef CONFIG_ESP_RMAKER_USER_ID_CHECK
            /* Store User Id in NVS since acknowledgement of the user-node association message is received */
            nvs_handle handle;
            esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, USER_MAPPING_NVS_NAMESPACE, NVS_READWRITE, &handle);
            if (err == ESP_OK) {
                nvs_set_blob(handle, USER_ID_NVS_NAME, rmaker_user_mapping_data->user_id, strlen(rmaker_user_mapping_data->user_id));
                nvs_close(handle);
            }
#endif
            esp_rmaker_user_mapping_cleanup_data();
            esp_event_handler_unregister(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_PUBLISHED,
                    &esp_rmaker_user_mapping_event_handler);
        }
        xSemaphoreGive(esp_rmaker_user_mapping_lock);
    }
}

static void esp_rmaker_user_mapping_mqtt_event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == RMAKER_COMMON_EVENT && event_id == RMAKER_MQTT_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "MQTT connected, queuing pending user mapping task.");

        /* Unregister this event handler as we only need it once */
        esp_event_handler_unregister(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_CONNECTED,
                &esp_rmaker_user_mapping_mqtt_event_handler);

        /* Queue the user mapping task now that MQTT is connected */
        if (esp_rmaker_work_queue_add_task(esp_rmaker_user_mapping_cb, NULL) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to queue user mapping task after MQTT connection. Starting retry mechanism.");
            user_mapping_schedule_retry();
        } else {
            ESP_LOGI(TAG, "Successfully queued user mapping task after MQTT connection.");
        }
    }
}

static void esp_rmaker_user_mapping_cb(void *priv_data)
{
    if (xSemaphoreTake(esp_rmaker_user_mapping_lock, SEMAPHORE_DELAY_MSEC/portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take semaphore.");
        return;
    }
    /* If there is no user node mapping data, or if the data is already sent, just return */
    if (rmaker_user_mapping_data == NULL || rmaker_user_mapping_data->sent == true) {
        xSemaphoreGive(esp_rmaker_user_mapping_lock);
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
    json_gen_obj_set_string(&jstr, "user_id", rmaker_user_mapping_data->user_id);
    json_gen_obj_set_string(&jstr, "secret_key", rmaker_user_mapping_data->secret_key);
    if (esp_rmaker_user_node_mapping_get_state() != ESP_RMAKER_USER_MAPPING_DONE) {
        json_gen_obj_set_bool(&jstr, "reset", true);
    }
    json_gen_end_object(&jstr);
    json_gen_str_end(&jstr);
    char publish_topic[MQTT_TOPIC_BUFFER_SIZE];
    esp_rmaker_create_mqtt_topic(publish_topic, sizeof(publish_topic), USER_MAPPING_TOPIC_SUFFIX, USER_MAPPING_TOPIC_RULE);
    esp_err_t err = esp_rmaker_mqtt_publish(publish_topic, publish_payload, strlen(publish_payload), RMAKER_MQTT_QOS1, &rmaker_user_mapping_data->mqtt_msg_id);
    ESP_LOGI(TAG, "MQTT Publish: %s", publish_payload);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MQTT Publish Error %d", err);
    } else {
        rmaker_user_mapping_state = ESP_RMAKER_USER_MAPPING_REQ_SENT;
        rmaker_user_mapping_data->sent = true;
    }
    xSemaphoreGive(esp_rmaker_user_mapping_lock);
    return;
}

/* This function only runs when esp_rmaker_start_user_node_mapping() is called,
 * which happens either during:
 * 1. Actual provisioning (when user provides credentials)
 * 2. Factory reset reporting (with dummy USER_RESET_ID)
 * It never runs during normal MQTT operation without provisioning.
 */
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
        nvs_user_id = MEM_CALLOC_EXTRAM(1, len + 1); /* +1 for NULL termination */
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
    /* If factory reset reporting is enabled but user ID check is disabled,
     * detect reset by checking if this is the dummy reset user ID */
#ifdef CONFIG_ESP_RMAKER_FACTORY_RESET_REPORTING
    if (strcmp(user_id, USER_RESET_ID) == 0) {
        return true;  /* This is a factory reset */
    }
#endif
    return false;
#endif
}

esp_err_t esp_rmaker_start_user_node_mapping(char *user_id, char *secret_key)
{
    if (esp_rmaker_user_mapping_lock == NULL) {
        ESP_LOGE(TAG, "User Node mapping not initialised.");
        return ESP_FAIL;
    }
    if (xSemaphoreTake(esp_rmaker_user_mapping_lock, SEMAPHORE_DELAY_MSEC/portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take semaphore.");
        return ESP_FAIL;
    }
    if (rmaker_user_mapping_data) {
        esp_rmaker_user_mapping_cleanup_data();
    }

    rmaker_user_mapping_data = MEM_CALLOC_EXTRAM(1, sizeof(esp_rmaker_user_mapping_data_t));
    if (!rmaker_user_mapping_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for rmaker_user_mapping_data.");
        xSemaphoreGive(esp_rmaker_user_mapping_lock);
        return ESP_ERR_NO_MEM;
    }
    rmaker_user_mapping_data->user_id = strdup(user_id);
    if (!rmaker_user_mapping_data->user_id) {
        ESP_LOGE(TAG, "Failed to allocate memory for user_id.");
        goto user_mapping_error;
    }
    rmaker_user_mapping_data->secret_key = strdup(secret_key);
    if (!rmaker_user_mapping_data->secret_key) {
        ESP_LOGE(TAG, "Failed to allocate memory for secret_key.");
        goto user_mapping_error;
    }
    if (esp_rmaker_user_mapping_detect_reset(user_id)) {
        ESP_LOGI(TAG, "User Node mapping reset detected.");
        rmaker_user_mapping_state = ESP_RMAKER_USER_MAPPING_STARTED;
    } else {
        rmaker_user_mapping_state = ESP_RMAKER_USER_MAPPING_DONE;
    }

    /* Try to queue the task immediately regardless of MQTT connection status */
    if (esp_rmaker_work_queue_add_task(esp_rmaker_user_mapping_cb, NULL) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue user mapping task. Checking MQTT connection status.");

        /* If MQTT is connected but queue failed, use retry mechanism */
        if (esp_rmaker_is_mqtt_connected()) {
            ESP_LOGW(TAG, "MQTT connected but queue full. Starting retry mechanism.");
            user_mapping_schedule_retry();
        } else {
            /* MQTT not connected, register for MQTT connected event */
            ESP_LOGI(TAG, "MQTT not connected, waiting for connection before retrying user mapping.");
            esp_err_t err = esp_event_handler_register(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_CONNECTED,
                    &esp_rmaker_user_mapping_mqtt_event_handler, NULL);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to register for MQTT connected event: %d", err);
                goto user_mapping_error;
            }
        }
    }
    esp_rmaker_user_mapping_prov_deinit();
    xSemaphoreGive(esp_rmaker_user_mapping_lock);
    return ESP_OK;

user_mapping_error:
    esp_rmaker_user_mapping_cleanup_data();
    xSemaphoreGive(esp_rmaker_user_mapping_lock);
    return ESP_FAIL;
}

#ifdef CONFIG_ESP_RMAKER_FACTORY_RESET_REPORTING
esp_err_t esp_rmaker_reset_user_node_mapping(void)
{
    return esp_rmaker_start_user_node_mapping(USER_RESET_ID, USER_RESET_KEY);
}
#endif

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
	    *outbuf = (uint8_t *)MEM_ALLOC_EXTRAM(*outlen);
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
    esp_err_t err = network_prov_mgr_endpoint_create(USER_MAPPING_ENDPOINT);
    return err;
}

esp_err_t esp_rmaker_user_mapping_endpoint_register(void)
{
    return network_prov_mgr_endpoint_register(USER_MAPPING_ENDPOINT, esp_rmaker_user_mapping_handler, NULL);
}

esp_err_t esp_rmaker_user_mapping_prov_init(void)
{
    int ret = ESP_OK;
    ret = esp_event_handler_register(NETWORK_PROV_EVENT, NETWORK_PROV_INIT, &esp_rmaker_user_mapping_event_handler, NULL);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_event_handler_register(NETWORK_PROV_EVENT, NETWORK_PROV_START, &esp_rmaker_user_mapping_event_handler, NULL);
    return ret;
}

esp_err_t esp_rmaker_user_mapping_prov_deinit(void)
{
    esp_event_handler_unregister(NETWORK_PROV_EVENT, NETWORK_PROV_INIT, &esp_rmaker_user_mapping_event_handler);
    esp_event_handler_unregister(NETWORK_PROV_EVENT, NETWORK_PROV_START, &esp_rmaker_user_mapping_event_handler);
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
    if (!esp_rmaker_user_mapping_lock) {
        esp_rmaker_user_mapping_lock = xSemaphoreCreateMutex();
        if (!esp_rmaker_user_mapping_lock) {
            ESP_LOGE(TAG, "Failed to create Mutex");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t esp_rmaker_user_node_mapping_deinit(void)
{
    /* Clean up retry state */
    user_mapping_retry_cleanup();

    /* Unregister any pending MQTT event handlers to prevent resource leak */
    esp_event_handler_unregister(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_CONNECTED,
            &esp_rmaker_user_mapping_mqtt_event_handler);

    if (esp_rmaker_user_mapping_lock) {
        vSemaphoreDelete(esp_rmaker_user_mapping_lock);
        esp_rmaker_user_mapping_lock = NULL;
    }
    return ESP_OK;
}
