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
#include <nvs_flash.h>
#include <nvs.h>

static const char *TAG = "esp_rmaker_storage";

#define CREDENTIALS_NAMESPACE           "rmaker_creds"

esp_err_t esp_rmaker_storage_init()
{
    static bool esp_rmaker_storage_init_done;
    if (esp_rmaker_storage_init_done) {
        ESP_LOGW(TAG, "ESP RainMaker Storage already initialised");
        return ESP_OK;
    }
    esp_err_t err = nvs_flash_init_partition(CONFIG_ESP_RMAKER_FACTORY_PARTITION_NAME);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS Flash init failed");
    } else {
        esp_rmaker_storage_init_done = true;
    }
    return err;
}

void *esp_rmaker_storage_get(const char *key)
{
    nvs_handle handle;
    esp_err_t err;
    if ((err = nvs_open_from_partition(CONFIG_ESP_RMAKER_FACTORY_PARTITION_NAME, CREDENTIALS_NAMESPACE,
                                NVS_READONLY, &handle)) != ESP_OK) {
        ESP_LOGD(TAG, "NVS open for %s %s %s failed with error %d", CONFIG_ESP_RMAKER_FACTORY_PARTITION_NAME, CREDENTIALS_NAMESPACE, key, err);
        return NULL;
    }
    size_t required_size = 0;
    if ((err = nvs_get_blob(handle, key, NULL, &required_size)) != ESP_OK) {
        ESP_LOGD(TAG, "Failed to read key %s with error %d size %d", key, err, required_size);
        nvs_close(handle);
        return NULL;
    }
    void *value = calloc(required_size + 1, 1); /* + 1 for NULL termination */
    if (value) {
        nvs_get_blob(handle, key, value, &required_size);
    }
    nvs_close(handle);
    return value;
}

esp_err_t esp_rmaker_storage_set(const char *key, void *data, size_t len)
{
    nvs_handle handle;
    esp_err_t err;
    if ((err = nvs_open_from_partition(CONFIG_ESP_RMAKER_FACTORY_PARTITION_NAME, CREDENTIALS_NAMESPACE,
                                NVS_READWRITE, &handle)) != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed with error %d", err);
        return ESP_FAIL;
    }
    if ((err = nvs_set_blob(handle, key, data, len)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write key %s with error %d size %d", key, err, len);
        nvs_close(handle);
        return ESP_FAIL;
    }
    nvs_commit(handle);
    nvs_close(handle);
    return ESP_OK;
}
