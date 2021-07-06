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

#include <sdkconfig.h>
#include <stdint.h>
#include <string.h>

#include <esp_rmaker_factory.h>
#include <esp_rmaker_core.h>

#include "esp_rmaker_internal.h"
#include "esp_rmaker_client_data.h"

extern uint8_t mqtt_server_root_ca_pem_start[] asm("_binary_rmaker_mqtt_server_crt_start");
extern uint8_t mqtt_server_root_ca_pem_end[] asm("_binary_rmaker_mqtt_server_crt_end");

char * esp_rmaker_get_mqtt_host()
{
    char *host = esp_rmaker_factory_get(ESP_RMAKER_MQTT_HOST_NVS_KEY);
#if defined(CONFIG_ESP_RMAKER_SELF_CLAIM) || defined(CONFIG_ESP_RMAKER_ASSISTED_CLAIM)
    if (!host) {
        return strdup(CONFIG_ESP_RMAKER_MQTT_HOST);
    }
#endif /* defined(CONFIG_ESP_RMAKER_SELF_CLAIM) || defined(CONFIG_ESP_RMAKER_ASSISTED_CLAIM) */
    return host;
}

char * esp_rmaker_get_client_cert()
{
    return esp_rmaker_factory_get(ESP_RMAKER_CLIENT_CERT_NVS_KEY);
}

char * esp_rmaker_get_client_key()
{
    return esp_rmaker_factory_get(ESP_RMAKER_CLIENT_KEY_NVS_KEY);
}

char * esp_rmaker_get_client_csr()
{
    return esp_rmaker_factory_get(ESP_RMAKER_CLIENT_CSR_NVS_KEY);
}

esp_rmaker_mqtt_conn_params_t *esp_rmaker_get_mqtt_conn_params()
{
    esp_rmaker_mqtt_conn_params_t *mqtt_conn_params = calloc(1, sizeof(esp_rmaker_mqtt_conn_params_t));
    if ((mqtt_conn_params->client_key = esp_rmaker_get_client_key()) == NULL) {
        goto init_err;
    }
    if ((mqtt_conn_params->client_cert = esp_rmaker_get_client_cert()) == NULL) {
        goto init_err;
    }
    if ((mqtt_conn_params->mqtt_host = esp_rmaker_get_mqtt_host()) == NULL) {
        goto init_err;
    }
    mqtt_conn_params->server_cert = (char *)mqtt_server_root_ca_pem_start;
    mqtt_conn_params->client_id = esp_rmaker_get_node_id();
    return mqtt_conn_params;
init_err:
    if (mqtt_conn_params->mqtt_host) {
        free(mqtt_conn_params->mqtt_host);
    }
    if (mqtt_conn_params->client_cert) {
        free(mqtt_conn_params->client_cert);
    }
    if (mqtt_conn_params->client_key) {
        free(mqtt_conn_params->client_key);
    }
    free(mqtt_conn_params);
    return NULL;
}

void esp_rmaker_clean_mqtt_conn_params(esp_rmaker_mqtt_conn_params_t *mqtt_conn_params)
{
    if (mqtt_conn_params) {
        if (mqtt_conn_params->mqtt_host) {
            free(mqtt_conn_params->mqtt_host);
        }
        if (mqtt_conn_params->client_cert) {
            free(mqtt_conn_params->client_cert);
        }
        if (mqtt_conn_params->client_key) {
            free(mqtt_conn_params->client_key);
        }
    }
}
