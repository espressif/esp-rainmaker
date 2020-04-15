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
#include <esp_log.h>
#include <esp_system.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_services.h>
#include <esp_rmaker_ota.h>

#include "esp_rmaker_ota_internal.h"
#include "esp_rmaker_mqtt.h"

static const char *TAG = "esp_rmaker_ota_using_params";

#define ESP_RMAKER_OTA_SERV_NAME    "ota"

void esp_rmaker_ota_finish(esp_rmaker_ota_t *ota)
{
    if (ota->url) {
        free(ota->url);
        ota->url = NULL;
    }
    ota->filesize = 0;
    ota->ota_in_progress = false;
}
static esp_err_t esp_rmaker_ota_service_cb(const char *dev_name, const char *name,
        esp_rmaker_param_val_t val, void *priv_data)
{
    esp_rmaker_ota_t *ota = (esp_rmaker_ota_t *)priv_data;
    if (!ota) {
        ESP_LOGE(TAG, "No OTA specific data received in callback");
        return ESP_FAIL;
    }
    if (ota->ota_in_progress) {
        ESP_LOGE(TAG, "OTA already in progress. Please try later.");
        return ESP_FAIL;
    }
    if (strcmp(name, ESP_RMAKER_DEF_OTA_URL_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.s, dev_name, name);
        if (ota->url) {
            free(ota->url);
            ota->url = NULL;
        }
        ota->url = strdup(val.val.s);
        if (ota->url) {
            ota->filesize = 0;
            ota->ota_in_progress = true;
            if (esp_rmaker_queue_work(esp_rmaker_ota_common_cb, ota) != ESP_OK) {
                esp_rmaker_ota_finish(ota);
            } else {
                return ESP_OK;
            }
        }

    }
    return ESP_FAIL;
}

esp_err_t esp_rmaker_ota_report_status_using_params(esp_rmaker_ota_handle_t ota_handle, ota_status_t status, char *additional_info)
{
    if (!ota_handle) {
        return ESP_FAIL;
    }
    esp_rmaker_update_param(ESP_RMAKER_OTA_SERV_NAME, ESP_RMAKER_DEF_OTA_STATUS_NAME,
            esp_rmaker_str(esp_rmaker_ota_status_to_string(status)));
    esp_rmaker_update_param(ESP_RMAKER_OTA_SERV_NAME, ESP_RMAKER_DEF_OTA_INFO_NAME,
            esp_rmaker_str(additional_info));
    return ESP_OK;
}

/* Enable the ESP RainMaker specific OTA */
esp_err_t esp_rmaker_ota_enable_using_params(esp_rmaker_ota_t *ota)
{
    esp_err_t err = esp_rmaker_create_ota_service(ESP_RMAKER_OTA_SERV_NAME, esp_rmaker_ota_service_cb, ota);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA enabled with Params");
    }
    return err;
}
