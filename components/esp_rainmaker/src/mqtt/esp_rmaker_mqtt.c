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
#include <stdio.h>
#include <string.h>
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <mqtt_client.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_mqtt.h>
#include <esp_rmaker_internal.h>

#include <esp_idf_version.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0)
// Features supported in 4.1

#ifdef CONFIG_ESP_RMAKER_MQTT_PORT_443
#define ESP_RMAKER_MQTT_USE_PORT_443
#endif

#else

#ifdef CONFIG_ESP_RMAKER_MQTT_PORT_443
#warning "Port 443 not supported in idf versions below 4.1. Using 8883 instead."
#endif

#endif /* !IDF4.1 */

static const char *TAG = "esp_rmaker_mqtt";

#define MAX_MQTT_SUBSCRIPTIONS      5

typedef struct {
    char *topic;
    esp_rmaker_mqtt_subscribe_cb_t cb;
    void *priv;
} esp_rmaker_mqtt_subscription_t;

typedef struct {
    esp_mqtt_client_handle_t mqtt_client;
    esp_rmaker_mqtt_config_t *config;
    esp_rmaker_mqtt_subscription_t *subscriptions[MAX_MQTT_SUBSCRIPTIONS];
} esp_rmaker_mqtt_data_t;
esp_rmaker_mqtt_data_t *mqtt_data;

const int MQTT_CONNECTED_EVENT = BIT1;
static EventGroupHandle_t mqtt_event_group;

typedef struct {
    char *data;
    char *topic;
} esp_rmaker_mqtt_long_data_t;

static void esp_rmaker_mqtt_subscribe_callback(const char *topic, int topic_len, const char *data, int data_len)
{
    esp_rmaker_mqtt_subscription_t **subscriptions = mqtt_data->subscriptions;
    int i;
    for (i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        if (subscriptions[i]) {
            if ((strncmp(topic, subscriptions[i]->topic, topic_len) == 0)
                    && (topic_len == strlen(subscriptions[i]->topic))) {
                subscriptions[i]->cb(subscriptions[i]->topic, (void *)data, data_len, subscriptions[i]->priv);
            }
        }
    }
}

esp_err_t esp_rmaker_mqtt_subscribe(const char *topic, esp_rmaker_mqtt_subscribe_cb_t cb, void *priv_data)
{
    if ( !mqtt_data || !topic || !cb) {
        return ESP_FAIL;
    }
    int i;
    for (i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        if (!mqtt_data->subscriptions[i]) {
            esp_rmaker_mqtt_subscription_t *subscription = calloc(1, sizeof(esp_rmaker_mqtt_subscription_t));
            if (!subscription) {
                return ESP_FAIL;
            }
            subscription->topic = strdup(topic);
            if (!subscription->topic) {
                free(subscription);
                return ESP_FAIL;
            }
            int ret = esp_mqtt_client_subscribe(mqtt_data->mqtt_client, subscription->topic, 1);
            if (ret < 0) {
                free(subscription->topic);
                free(subscription);
                return ESP_FAIL;
            }
            subscription->priv = priv_data;
            subscription->cb = cb;
            mqtt_data->subscriptions[i] = subscription;
            ESP_LOGD(TAG, "Subscribed to topic: %s", topic);
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

esp_err_t esp_rmaker_mqtt_unsubscribe(const char *topic)
{
    if (!mqtt_data || !topic) {
        return ESP_FAIL;
    }
    int ret = esp_mqtt_client_unsubscribe(mqtt_data->mqtt_client, topic);
    if (ret < 0) {
        ESP_LOGW(TAG, "Could not unsubscribe from topic: %s", topic);
    }
    esp_rmaker_mqtt_subscription_t **subscriptions = mqtt_data->subscriptions;
    int i;
    for (i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        if (subscriptions[i]) {
            if (strncmp(topic, subscriptions[i]->topic, strlen(topic)) == 0) {
                free(subscriptions[i]->topic);
                free(subscriptions[i]);
                subscriptions[i] = NULL;
                return ESP_OK;
            }
        }
    }
    return ESP_FAIL;
}

esp_err_t esp_rmaker_mqtt_publish(const char *topic, void *data, size_t data_len)
{
    if (!mqtt_data || !topic || !data) {
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "Publishing to %s", topic);
    int ret = esp_mqtt_client_publish(mqtt_data->mqtt_client, topic, data, data_len, 1, 0);
    if (ret < 0) {
        ESP_LOGE(TAG, "MQTT Publish failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_rmaker_mqtt_long_data_t *esp_rmaker_mqtt_free_long_data(esp_rmaker_mqtt_long_data_t *long_data)
{
    if (long_data) {
        if (long_data->topic) {
            free(long_data->topic);
        }
        if (long_data->data) {
            free(long_data->data);
        }
        free(long_data);
    }
    return NULL;
}

static esp_rmaker_mqtt_long_data_t *esp_rmaker_mqtt_manage_long_data(esp_rmaker_mqtt_long_data_t *long_data,
        esp_mqtt_event_handle_t event)
{
    if (event->topic) {
        /* This is new data. Free any earlier data, if present. */
        esp_rmaker_mqtt_free_long_data(long_data);
        long_data = calloc(1, sizeof(esp_rmaker_mqtt_long_data_t));
        if (!long_data) {
            ESP_LOGE(TAG, "Could not allocate memory for esp_rmaker_mqtt_long_data_t");
            return NULL;
        }
        long_data->data = calloc(1, event->total_data_len);
        if (!long_data->data) {
            ESP_LOGE(TAG, "Could not allocate %d bytes for received data.", event->total_data_len);
            return esp_rmaker_mqtt_free_long_data(long_data);
        }
        long_data->topic = strndup(event->topic, event->topic_len);
        if (!long_data->topic) {
            ESP_LOGE(TAG, "Could not allocate %d bytes for received topic.", event->topic_len);
            return esp_rmaker_mqtt_free_long_data(long_data);
        }
    }
    if (long_data) {
        memcpy(long_data->data + event->current_data_offset, event->data, event->data_len);

        if ((event->current_data_offset + event->data_len) == event->total_data_len) {
            esp_rmaker_mqtt_subscribe_callback(long_data->topic, strlen(long_data->topic),
                        long_data->data, event->total_data_len);
            return esp_rmaker_mqtt_free_long_data(long_data);
        }
    }
    return long_data;
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            /* Resubscribe to all topics after reconnection */
            for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
                if (mqtt_data->subscriptions[i]) {
                    esp_mqtt_client_subscribe(event->client, mqtt_data->subscriptions[i]->topic, 1);
                }
            }
            xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_EVENT);
            esp_rmaker_post_event(RMAKER_EVENT_MQTT_CONNECTED, NULL, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Disconnected. Will try reconnecting in a while...");
            esp_rmaker_post_event(RMAKER_EVENT_MQTT_DISCONNECTED, NULL, 0);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            esp_rmaker_post_event(RMAKER_EVENT_MQTT_PUBLISHED,
                    &event->msg_id, sizeof(event->msg_id));
            break;
        case MQTT_EVENT_DATA: {
            ESP_LOGD(TAG, "MQTT_EVENT_DATA");
            static esp_rmaker_mqtt_long_data_t *long_data;
            /* Topic can be NULL, for data longer than the MQTT buffer */
            if (event->topic) {
                ESP_LOGD(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
            }
            ESP_LOGD(TAG, "DATA=%.*s\r\n", event->data_len, event->data);
            if (event->data_len == event->total_data_len) {
                /* If long_data still exists, it means there was some issue getting the
                 * long data, and so, it needs to be freed up.
                 */
                if (long_data) {
                    long_data = esp_rmaker_mqtt_free_long_data(long_data);
                }
                esp_rmaker_mqtt_subscribe_callback(event->topic, event->topic_len, event->data, event->data_len);
            } else {
                long_data = esp_rmaker_mqtt_manage_long_data(long_data, event);
            }
            break;
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGD(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}
esp_err_t esp_rmaker_mqtt_connect(void)
{
    if (!mqtt_data) {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Connecting to %s", mqtt_data->config->mqtt_host);
    mqtt_event_group = xEventGroupCreate();
    esp_err_t ret = esp_mqtt_client_start(mqtt_data->mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start() failed with err = %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "Waiting for MQTT connection. This may take time.");
    xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_EVENT, false, true, portMAX_DELAY);
    return ESP_OK;
}

static void esp_rmaker_mqtt_unsubscribe_all()
{
    if (!mqtt_data) {
        return;
    }
    int i;
    for (i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        if (mqtt_data->subscriptions[i]) {
            esp_rmaker_mqtt_unsubscribe(mqtt_data->subscriptions[i]->topic);
        }
    }
}

esp_err_t esp_rmaker_mqtt_disconnect(void)
{
    if (!mqtt_data) {
        return ESP_FAIL;
    }
    esp_rmaker_mqtt_unsubscribe_all();
    esp_err_t err = esp_mqtt_client_stop(mqtt_data->mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect from MQTT");
    } else {
        ESP_LOGI(TAG, "MQTT Disconnected.");
    }
    return err;
}
#ifdef ESP_RMAKER_MQTT_USE_PORT_443
static const char *alpn_protocols[] = { "x-amzn-mqtt-ca", NULL };
#endif /* ESP_RMAKER_MQTT_USE_PORT_443 */
esp_err_t esp_rmaker_mqtt_init(esp_rmaker_mqtt_config_t *config)
{
    if (mqtt_data) {
        ESP_LOGE(TAG, "MQTT already initialised");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Initialising MQTT");
    mqtt_data = calloc(1, sizeof(esp_rmaker_mqtt_data_t));
    if (!mqtt_data) {
        return ESP_FAIL;
    }
    mqtt_data->config = config;
    const esp_mqtt_client_config_t mqtt_client_cfg = {
        .host = config->mqtt_host,
#ifdef ESP_RMAKER_MQTT_USE_PORT_443
        .port = 443,
        .alpn_protos = alpn_protocols,
#else
        .port = 8883,
#endif /* !ESP_RMAKER_MQTT_USE_PORT_443 */
        .cert_pem = (const char *)config->server_cert,
        .client_cert_pem = (const char *)config->client_cert,
        .client_key_pem = (const char *)config->client_key,
        .client_id = (const char *)config->client_id,
        .keepalive = 120,
        .event_handle = mqtt_event_handler,
        .transport = MQTT_TRANSPORT_OVER_SSL,
#ifdef CONFIG_RMAKER_MQTT_PERSISTENT_SESSION
        .disable_clean_session = 1,
#endif /* CONFIG_RMAKER_MQTT_PERSISTENT_SESSION */
    };
    mqtt_data->mqtt_client = esp_mqtt_client_init(&mqtt_client_cfg);
    return ESP_OK;
}
