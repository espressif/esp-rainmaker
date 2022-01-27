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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_https_ota.h>
#include <esp_wifi_types.h>
#include <esp_wifi.h>
#if CONFIG_BT_ENABLED
#include <esp_bt.h>
#endif /* CONFIG_BT_ENABLED */

#include <esp_rmaker_utils.h>
#include "esp_rmaker_ota_internal.h"

static const char *TAG = "esp_rmaker_ota";

#define OTA_REBOOT_TIMER_SEC    10
#define DEF_HTTP_TX_BUFFER_SIZE    1024
#define DEF_HTTP_RX_BUFFER_SIZE    CONFIG_ESP_RMAKER_OTA_HTTP_RX_BUFFER_SIZE

extern const char esp_rmaker_ota_def_cert[] asm("_binary_rmaker_ota_server_crt_start");
const char *ESP_RMAKER_OTA_DEFAULT_SERVER_CERT = esp_rmaker_ota_def_cert;
char *esp_rmaker_ota_status_to_string(ota_status_t status)
{
    switch (status) {
        case OTA_STATUS_IN_PROGRESS:
            return "in-progress";
        case OTA_STATUS_SUCCESS:
            return "success";
        case OTA_STATUS_FAILED:
            return "failed";
        case OTA_STATUS_DELAYED:
            return "delayed";
        default:
            return "invalid";
    }
    return "invalid";
}
esp_err_t esp_rmaker_ota_report_status(esp_rmaker_ota_handle_t ota_handle, ota_status_t status, char *additional_info)
{
    if (!ota_handle) {
        return ESP_FAIL;
    }
    esp_rmaker_ota_t *ota = (esp_rmaker_ota_t *)ota_handle;
    esp_err_t err = ESP_FAIL;
    if (ota->type == OTA_USING_PARAMS) {
        err = esp_rmaker_ota_report_status_using_params(ota_handle, status, additional_info);
    } else if (ota->type == OTA_USING_TOPICS) {
        err = esp_rmaker_ota_report_status_using_topics(ota_handle, status, additional_info);
    }
    if (err == ESP_OK) {
        esp_rmaker_ota_t *ota = (esp_rmaker_ota_t *)ota_handle;
        ota->last_reported_status = status;
    }
    return err;
}

void esp_rmaker_ota_common_cb(void *priv)
{
    if (!priv) {
        return;
    }
    esp_rmaker_ota_t *ota = (esp_rmaker_ota_t *)priv;
    if (!ota->url) {
        goto ota_finish;
    }
    esp_rmaker_ota_data_t ota_data = {
        .url = ota->url,
        .filesize = ota->filesize,
        .server_cert = ota->server_cert,
        .priv = ota->priv
    };
    ota->ota_cb((esp_rmaker_ota_handle_t) ota, &ota_data);
ota_finish:
    if (ota->type == OTA_USING_PARAMS) {
        esp_rmaker_ota_finish_using_params(ota);
    } else if (ota->type == OTA_USING_TOPICS) {
        esp_rmaker_ota_finish_using_topics(ota);
    }
}

static esp_err_t validate_image_header(esp_rmaker_ota_handle_t ota_handle,
        esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGD(TAG, "Running firmware version: %s", running_app_info.version);
    }

#ifndef CONFIG_ESP_RMAKER_SKIP_VERSION_CHECK
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0) {
        ESP_LOGW(TAG, "Current running version is same as the new. We will not continue the update.");
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Same version received");
        return ESP_FAIL;
    }
#endif

#ifndef CONFIG_ESP_RMAKER_SKIP_PROJECT_NAME_CHECK
    if (memcmp(new_app_info->project_name, running_app_info.project_name, sizeof(new_app_info->project_name)) != 0) {
        ESP_LOGW(TAG, "OTA Image built for Project: %s. Expected: %s",
                new_app_info->project_name, running_app_info.project_name);
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Project Name mismatch");
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

static esp_err_t esp_rmaker_ota_default_cb(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data)
{
    if (!ota_data->url) {
        return ESP_FAIL;
    }
    int buffer_size_tx = DEF_HTTP_TX_BUFFER_SIZE;
    /* In case received url is longer, we will increase the tx buffer size
     * to accomodate the longer url and other headers.
     */
    if (strlen(ota_data->url) > buffer_size_tx) {
        buffer_size_tx = strlen(ota_data->url) + 128;
    }
    esp_err_t ota_finish_err = ESP_OK;
    esp_http_client_config_t config = {
        .url = ota_data->url,
        .cert_pem = ota_data->server_cert,
        .timeout_ms = 5000,
        .buffer_size = DEF_HTTP_RX_BUFFER_SIZE,
        .buffer_size_tx = buffer_size_tx
    };
#ifdef CONFIG_ESP_RMAKER_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    if (ota_data->filesize) {
        ESP_LOGD(TAG, "Received file size: %d", ota_data->filesize);
    }

    esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_IN_PROGRESS, "Starting OTA Upgrade");

    /* Using a warning just to highlight the message */
    ESP_LOGW(TAG, "Starting OTA. This may take time.");
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "ESP HTTPS OTA Begin failed");
        return ESP_FAIL;
    }

/* Get the current Wi-Fi power save type. In case OTA fails and we need this
 * to restore power saving.
 */
    wifi_ps_type_t ps_type;
    esp_wifi_get_ps(&ps_type);
/* Disable Wi-Fi power save to speed up OTA, iff BT is controller is idle/disabled.
 * Co-ex requirement, device panics otherwise.*/
#if CONFIG_BT_ENABLED
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
        esp_wifi_set_ps(WIFI_PS_NONE);
    }
#else
    esp_wifi_set_ps(WIFI_PS_NONE);
#endif /* CONFIG_BT_ENABLED */

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed");
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Failed to read image decription");
        goto ota_end;
    }
    err = validate_image_header(ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "image header verification failed");
        goto ota_end;
    }

    esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_IN_PROGRESS, "Downloading Firmware Image");
    int count = 0;
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        /* esp_https_ota_perform returns after every read operation which gives user the ability to
         * monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
         * data read so far.
         * We are using a counter just to reduce the number of prints
         */

        count++;
        if (count == 50) {
            ESP_LOGI(TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
            count = 0;
        }
    }
    if (err != ESP_OK) {
        char description[40];
        snprintf(description, sizeof(description), "OTA failed: Error %s", esp_err_to_name(err));
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, description);
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG, "Complete data was not received.");
    } else {
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_IN_PROGRESS, "Firmware Image download complete");
    }

ota_end:
#ifdef CONFIG_BT_ENABLED
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
        esp_wifi_set_ps(ps_type);
    }
#else
    esp_wifi_set_ps(ps_type);
#endif /* CONFIG_BT_ENABLED */
    ota_finish_err = esp_https_ota_finish(https_ota_handle);
    if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
        ESP_LOGI(TAG, "OTA upgrade successful. Rebooting in %d seconds...", OTA_REBOOT_TIMER_SEC);
        esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_SUCCESS, "OTA Upgrade finished successfully");
        esp_rmaker_reboot(OTA_REBOOT_TIMER_SEC);
        return ESP_OK;
    } else {
        if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_FAILED, "Image validation failed");
        } else {
            /* Not reporting status here, because relevant error will already be reported
             * in some earlier step
             */
            ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed %d", ota_finish_err);
        }
    }
    return ESP_FAIL;
}

/* Enable the ESP RainMaker specific OTA */
esp_err_t esp_rmaker_ota_enable(esp_rmaker_ota_config_t *ota_config, esp_rmaker_ota_type_t type)
{
    if (!ota_config || ((type != OTA_USING_PARAMS) && (type != OTA_USING_TOPICS))) {
        ESP_LOGE(TAG,"Invalid arguments for esp_rmaker_ota_enable()");
        return ESP_ERR_INVALID_ARG;
    }
    static bool ota_init_done;
    if (ota_init_done) {
        ESP_LOGE(TAG, "OTA already initialised");
        return ESP_FAIL;
    }
    esp_rmaker_ota_t *ota = calloc(1, sizeof(esp_rmaker_ota_t));
    if (!ota) {
        ESP_LOGE(TAG, "Failed to allocate memory for esp_rmaker_ota_t");
        return ESP_ERR_NO_MEM;
    }
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
            ESP_LOGI(TAG, "OTA state = %d", ota_state);
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "First Boot after an OTA");
            /* Run diagnostic function */
            bool diagnostic_is_ok = true;
            if (ota_config->ota_diag) {
                diagnostic_is_ok = ota_config->ota_diag();
            }
            if (diagnostic_is_ok) {
                ESP_LOGI(TAG, "Diagnostics completed successfully! Continuing execution ...");
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                ESP_LOGE(TAG, "Diagnostics failed! Start rollback to the previous version ...");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
    }
    if (ota_config->ota_cb) {
        ota->ota_cb = ota_config->ota_cb;
    } else {
        ota->ota_cb = esp_rmaker_ota_default_cb;
    }
    ota->priv = ota_config->priv;
    ota->server_cert = ota_config->server_cert;
    esp_err_t err = ESP_FAIL;
    ota->type = type;
    if (type == OTA_USING_PARAMS) {
        err = esp_rmaker_ota_enable_using_params(ota);
    } else if (type == OTA_USING_TOPICS) {
        err = esp_rmaker_ota_enable_using_topics(ota);
    }
    if (err == ESP_OK) {
        ota_init_done = true;
    } else {
        free(ota);
        ESP_LOGE(TAG, "Failed to enable OTA");
    }
    return err;
}
