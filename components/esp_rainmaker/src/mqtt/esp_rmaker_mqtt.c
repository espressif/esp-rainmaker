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

#include <esp_log.h>
#include <esp_rmaker_mqtt_glue.h>
#include <esp_rmaker_client_data.h>
static const char *TAG = "esp_rmaker_mqtt";
static esp_rmaker_mqtt_config_t g_mqtt_config;

esp_rmaker_mqtt_conn_params_t *esp_rmaker_mqtt_get_conn_params(void)
{
    if (g_mqtt_config.get_conn_params) {
        return g_mqtt_config.get_conn_params();
    } else {
        return esp_rmaker_get_mqtt_conn_params();
    }
}

esp_err_t esp_rmaker_mqtt_setup(esp_rmaker_mqtt_config_t mqtt_config)
{
    g_mqtt_config = mqtt_config;
    g_mqtt_config.setup_done  = true;
    return ESP_OK;
}

esp_err_t esp_rmaker_mqtt_init(esp_rmaker_mqtt_conn_params_t *conn_params)
{
    if (!g_mqtt_config.setup_done) {
        esp_rmaker_mqtt_glue_setup(&g_mqtt_config);
    }
    if (g_mqtt_config.init) {
        return g_mqtt_config.init(conn_params);
    }
    ESP_LOGW(TAG, "esp_rmaker_mqtt_init not registered");
    return ESP_OK;
}

esp_err_t esp_rmaker_mqtt_connect(void)
{
    if (g_mqtt_config.connect) {
        return g_mqtt_config.connect();
    }
    ESP_LOGW(TAG, "esp_rmaker_mqtt_connect not registered");
    return ESP_OK;
}

esp_err_t esp_rmaker_mqtt_disconnect(void)
{
    if (g_mqtt_config.disconnect) {
        return g_mqtt_config.disconnect();
    }
    ESP_LOGW(TAG, "esp_rmaker_mqtt_disconnect not registered");
    return ESP_OK;
}

esp_err_t esp_rmaker_mqtt_subscribe(const char *topic, esp_rmaker_mqtt_subscribe_cb_t cb, uint8_t qos, void *priv_data)
{
    if (g_mqtt_config.subscribe) {
        return g_mqtt_config.subscribe(topic, cb, qos, priv_data);
    }
    ESP_LOGW(TAG, "esp_rmaker_mqtt_subscribe not registered");
    return ESP_OK;
}

esp_err_t esp_rmaker_mqtt_unsubscribe(const char *topic)
{
    if (g_mqtt_config.unsubscribe) {
        return g_mqtt_config.unsubscribe(topic);
    }
    ESP_LOGW(TAG, "esp_rmaker_mqtt_unsubscribe not registered");
    return ESP_OK;
}

esp_err_t esp_rmaker_mqtt_publish(const char *topic, void *data, size_t data_len, uint8_t qos, int *msg_id)
{
    if (g_mqtt_config.publish) {
        return g_mqtt_config.publish(topic, data, data_len, qos, msg_id);
    }
    ESP_LOGW(TAG, "esp_rmaker_mqtt_publish not registered");
    return ESP_OK;
}
