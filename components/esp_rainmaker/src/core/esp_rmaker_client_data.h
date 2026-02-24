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
#pragma once
#include <stdint.h>
#include <esp_rmaker_mqtt_glue.h>

#define ESP_RMAKER_CLIENT_CERT_NVS_KEY   "client_cert"
#define ESP_RMAKER_CLIENT_KEY_NVS_KEY    "client_key"
#define ESP_RMAKER_MQTT_HOST_NVS_KEY     "mqtt_host"
#define ESP_RMAKER_CLIENT_CSR_NVS_KEY    "csr"
#define ESP_RMAKER_CLIENT_RANDOM_NVS_KEY "random"

/** Returns the client certificate
 *
 * @return char pointer to the client certificate.
 * @note Free the memory after use using free().
 */
char *esp_rmaker_get_client_cert();

/** Returns the length of the client certificate
 *
 * @return size_t length of the client certificate.
 */
size_t esp_rmaker_get_client_cert_len();

/** Returns the client key
 *
 * @return char pointer to the client key.
 * @note Free the memory after use using free().
 */
char *esp_rmaker_get_client_key();

/** Returns the length of the client key
 *
 * @return size_t length of the client key.
 */
size_t esp_rmaker_get_client_key_len();

/** Returns the client CSR
 *
 * @return char pointer to the client CSR.
 * @note Free the memory after use using free().
 */
char *esp_rmaker_get_client_csr();

/** Returns the length of the client CSR
 *
 * @return size_t length of the client CSR.
 */
size_t esp_rmaker_get_client_csr_len();

/** Returns the MQTT host
 *
 * @return char pointer to the MQTT host.
 * @note Free the memory after use using free().
 */
char *esp_rmaker_get_mqtt_host();

/** Returns the MQTT connection parameters
 *
 * @return esp_rmaker_mqtt_conn_params_t pointer to the MQTT connection parameters.
 * @note Free the mqtt connection parameters after use using esp_rmaker_clean_mqtt_conn_params().
 */
esp_rmaker_mqtt_conn_params_t *esp_rmaker_get_mqtt_conn_params();

/** Cleans the MQTT connection parameters
 *
 * @param mqtt_conn_params pointer to the MQTT connection parameters, returned by esp_rmaker_get_mqtt_conn_params().
 */
void esp_rmaker_clean_mqtt_conn_params(esp_rmaker_mqtt_conn_params_t *mqtt_conn_params);

/** Set LWT data for MQTT connection params
 *
 * This sets the LWT data that will be used when esp_rmaker_get_mqtt_conn_params()
 * is called. The data is copied internally.
 *
 * @param[in] topic The LWT topic. Set to NULL to clear LWT.
 * @param[in] message The LWT message payload.
 * @param[in] message_len Length of the LWT message.
 *
 * @return ESP_OK on success.
 * @return ESP_ERR_NO_MEM if memory allocation fails.
 */
esp_err_t esp_rmaker_set_mqtt_conn_lwt(const char *topic, const char *message, size_t message_len);
