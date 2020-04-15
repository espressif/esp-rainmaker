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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <mqtt_client.h>

#include <esp_rmaker_mqtt.h>

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

static void esp_rmaker_mqtt_subscribe_callback(const char *topic, int topic_len, const char *data, int data_len)
{
    esp_rmaker_mqtt_subscription_t **subscriptions = mqtt_data->subscriptions;
    int i;
    for (i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        if (subscriptions[i]) {
            if (strncmp(topic, subscriptions[i]->topic, topic_len) == 0) {
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

esp_err_t esp_rmaker_mqtt_publish(const char *topic, const char *data)
{
    if (!mqtt_data || !topic || !data) {
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "Publishing to %s", topic);
    int ret = esp_mqtt_client_publish(mqtt_data->mqtt_client, topic, data, strlen(data), 1, 0);
    if (ret < 0) {
        ESP_LOGE(TAG, "MQTT Publish failed");
        return ESP_FAIL;
    }
    return ESP_OK;
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
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Disconnected");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGD(TAG, "MQTT_EVENT_DATA");
            ESP_LOGD(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
            ESP_LOGD(TAG, "DATA=%.*s\r\n", event->data_len, event->data);
            esp_rmaker_mqtt_subscribe_callback(event->topic, event->topic_len, event->data, event->data_len);
            break;
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
        .port = 8883,
        .cert_pem = (const char *)config->server_cert,
        .client_cert_pem = (const char *)config->client_cert,
        .client_key_pem = (const char *)config->client_key,
        .client_id = (const char *)config->client_id,
        .keepalive = 120,
        .event_handle = mqtt_event_handler,
        .transport = MQTT_TRANSPORT_OVER_SSL,
    };
    mqtt_data->mqtt_client = esp_mqtt_client_init(&mqtt_client_cfg);
    return ESP_OK;
}
