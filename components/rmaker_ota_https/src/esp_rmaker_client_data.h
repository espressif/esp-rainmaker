/*
 * SPDX-FileCopyrightText: 2020 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <stdint.h>
#include <esp_rmaker_mqtt_glue.h>

#define ESP_RMAKER_CLIENT_CERT_NVS_KEY   "client_cert"
#define ESP_RMAKER_CLIENT_KEY_NVS_KEY    "client_key"
#define ESP_RMAKER_MQTT_HOST_NVS_KEY     "mqtt_host"
#define ESP_RMAKER_CLIENT_CSR_NVS_KEY    "csr"
#define ESP_RMAKER_CLIENT_RANDOM_NVS_KEY "random"

char *esp_rmaker_get_client_cert();
size_t esp_rmaker_get_client_cert_len();
char *esp_rmaker_get_client_key();
size_t esp_rmaker_get_client_key_len();
char *esp_rmaker_get_client_csr();
char *esp_rmaker_get_mqtt_host();
esp_rmaker_mqtt_conn_params_t *esp_rmaker_get_mqtt_conn_params();
void esp_rmaker_clean_mqtt_conn_params(esp_rmaker_mqtt_conn_params_t *mqtt_conn_params);
