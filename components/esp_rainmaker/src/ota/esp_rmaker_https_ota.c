/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_wifi_types.h>
#include <esp_wifi.h>
#include <errno.h>
#include <nvs.h>
#include <esp_https_ota.h>

#ifdef CONFIG_ESP_RMAKER_USE_CERT_BUNDLE
#include <esp_crt_bundle.h>
#endif

#include <esp_rmaker_utils.h>
#include <esp_rmaker_common_events.h>
#include "esp_rmaker_internal.h"
#include "esp_rmaker_ota_internal.h"
#include "esp_rmaker_https_ota.h"

static const char *TAG = "esp_rmaker_https_ota";

/* HTTPS-specific certificate definition */
extern const char esp_rmaker_ota_def_cert[] asm("_binary_rmaker_ota_server_crt_start");
const char *ESP_RMAKER_OTA_DEFAULT_SERVER_CERT = esp_rmaker_ota_def_cert;

#define DEF_HTTP_TX_BUFFER_SIZE    1024
#ifdef CONFIG_ESP_RMAKER_OTA_HTTP_RX_BUFFER_SIZE
#define DEF_HTTP_RX_BUFFER_SIZE    CONFIG_ESP_RMAKER_OTA_HTTP_RX_BUFFER_SIZE
#else
#define DEF_HTTP_RX_BUFFER_SIZE    1024
#endif

/* Local macro for OTA max retries, can be overridden for development */
#ifndef ESP_RMAKER_OTA_MAX_RETRIES
#define ESP_RMAKER_OTA_MAX_RETRIES   CONFIG_ESP_RMAKER_OTA_MAX_RETRIES
#endif

/* Local macro for OTA retry delay in seconds, can be overridden for development */
#ifndef ESP_RMAKER_OTA_RETRY_DELAY_SECONDS
#define ESP_RMAKER_OTA_RETRY_DELAY_SECONDS   (CONFIG_ESP_RMAKER_OTA_RETRY_DELAY_MINUTES * 60)
#endif

#define ESP_RMAKER_HTTPS_OTA_TIMEOUT_MS 5000

#ifdef CONFIG_ESP_RMAKER_HTTP_OTA_RESUMPTION
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
#define RMAKER_OTA_HTTP_OTA_RESUMPTION
#else
#warning "HTTP OTA resumption, needs IDF version >= 5.5.0"
#endif
#endif

#ifdef RMAKER_OTA_HTTP_OTA_RESUMPTION
#define RMAKER_OTA_WRITTEN_LENGTH_NVS_NAME  "ota_writen"
#define RMAKER_OTA_FILE_MD5_NVS_NAME  "ota_file_md5"
static esp_err_t esp_rmaker_https_ota_get_len_and_md5_from_nvs(uint32_t *written_len, char **file_md5)
{
    *written_len = 0;
    *file_md5 = NULL;
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, RMAKER_OTA_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        uint32_t len = 0;
        err = nvs_get_u32(handle, RMAKER_OTA_WRITTEN_LENGTH_NVS_NAME, &len);
        if (err == ESP_OK) {
            *written_len = len;
            size_t file_md5_len = 0;
            err = nvs_get_str(handle, RMAKER_OTA_FILE_MD5_NVS_NAME, NULL, &file_md5_len);
            if (err == ESP_OK) {
                *file_md5 = MEM_CALLOC_EXTRAM(1, file_md5_len + 1);
                if (!*file_md5) {
                    err = ESP_ERR_NO_MEM;
                } else {
                    err = nvs_get_str(handle, RMAKER_OTA_FILE_MD5_NVS_NAME, *file_md5, &file_md5_len);
                    if (err == ESP_OK) {
                        (*file_md5)[file_md5_len] = '\0';
                    } else {
                        free(*file_md5);
                        *file_md5 = NULL;
                    }
                }
            }
        }
        nvs_close(handle);
    }
    return err;
}

static esp_err_t esp_rmaker_https_ota_set_len_and_md5_to_nvs(uint32_t written_len, char *file_md5)
{
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, RMAKER_OTA_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, RMAKER_OTA_WRITTEN_LENGTH_NVS_NAME, written_len);
        if (err == ESP_OK) {
            if (file_md5) {
                err = nvs_set_str(handle, RMAKER_OTA_FILE_MD5_NVS_NAME, file_md5);
            }
            if (err == ESP_OK) {
                nvs_commit(handle);
            }
        }
        nvs_close(handle);
    }
    return err;
}

static esp_err_t esp_rmaker_https_ota_cleanup_ota_cfg_from_nvs(void)
{
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, RMAKER_OTA_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_key(handle, RMAKER_OTA_WRITTEN_LENGTH_NVS_NAME);
        nvs_erase_key(handle, RMAKER_OTA_FILE_MD5_NVS_NAME);
        nvs_commit(handle);
        nvs_close(handle);
    }
    return err;
}
#endif

static esp_err_t esp_rmaker_ota_use_https(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data, char *err_desc, size_t err_desc_size)
{
    int buffer_size_tx = DEF_HTTP_TX_BUFFER_SIZE;
    /* In case received url is longer, we will increase the tx buffer size
     * to accomodate the longer url and other headers.
     */
    if (strlen(ota_data->url) > buffer_size_tx) {
        buffer_size_tx = strlen(ota_data->url) + 128;
    }

    if (ota_data->filesize) {
        ESP_LOGD(TAG, "Received file size: %d", ota_data->filesize);
    }

    esp_err_t ota_finish_err = ESP_OK;
    esp_http_client_config_t config = {
        .timeout_ms = ESP_RMAKER_HTTPS_OTA_TIMEOUT_MS,
        .url = ota_data->url,
#ifdef CONFIG_ESP_RMAKER_USE_CERT_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#else
        .cert_pem = ota_data->server_cert ? ota_data->server_cert : ESP_RMAKER_OTA_DEFAULT_SERVER_CERT,
#endif
#ifdef CONFIG_ESP_RMAKER_SKIP_COMMON_NAME_CHECK
        .skip_cert_common_name_check = true,
#endif
        .buffer_size = DEF_HTTP_RX_BUFFER_SIZE,
        .buffer_size_tx = buffer_size_tx,
        .keep_alive_enable = true
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
#ifdef RMAKER_OTA_HTTP_OTA_RESUMPTION
    /* Check if file md5 is present and match with the one in the ota_data, if yes, resume the OTA;
    otherwise, start from the beginning and set the written length 0 and file md5 to NVS */
    if (ota_data->file_md5) {
        char *file_md5 = NULL;
        uint32_t written_len = 0;
        bool resume_ota = false;
        if (esp_rmaker_https_ota_get_len_and_md5_from_nvs(&written_len, &file_md5) == ESP_OK) {
            if (strncmp(file_md5, ota_data->file_md5, strlen(ota_data->file_md5)) != 0) {
                ESP_LOGW(TAG, "File MD5 mismatch, seems a new firmware, not resuming OTA");
            } else {
                resume_ota = true;
                ota_config.ota_resumption = true;
                ota_config.ota_image_bytes_written = written_len;
            }
            free(file_md5);
        }
        if (!resume_ota) {
            /* Start from the beginning and set the written length 0 and file md5 to NVS */
            if (esp_rmaker_https_ota_set_len_and_md5_to_nvs(0, ota_data->file_md5) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set written length 0 and file MD5 to NVS");
            }
        }
    } else {
        ESP_LOGW(TAG, "File MD5 not present, not resuming OTA because can not verify the already downloaded firmware is the same as the new firmware, please upgrade the backend to support it");
    }
#endif
    /* Using a warning just to highlight the message */
    ESP_LOGW(TAG, "Starting OTA. This may take time.");
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        int err_no = errno;
        snprintf(err_desc, err_desc_size, "OTA Begin failed: %s (errno=%d: %s)", esp_err_to_name(err), err_no, err_no ? strerror(err_no) : "Invalid");
        return ESP_FAIL;
    }

#ifdef CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI
/* Get the current Wi-Fi power save type. In case OTA fails and we need this
 * to restore power saving.
 */
    wifi_ps_type_t ps_type;
    esp_wifi_get_ps(&ps_type);
/* Disable Wi-Fi power save to speed up OTA, iff BT is controller is idle/disabled.
 * Co-ex requirement, device panics otherwise.*/
#if defined(RMAKER_OTA_BT_ENABLED_CHECK)
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
        esp_wifi_set_ps(WIFI_PS_NONE);
    }
#else
    esp_wifi_set_ps(WIFI_PS_NONE);
#endif /* RMAKER_OTA_BT_ENABLED_CHECK */
#endif /* CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI */

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);

    if (err != ESP_OK) {
        int err_no = errno;
        snprintf(err_desc, err_desc_size, "Failed to read image description: %s (errno=%d: %s)", esp_err_to_name(err), err_no, err_no ? strerror(err_no) : "Invalid");
        /* OTA failed, may retry later */
        goto ota_end;
    }
    err = validate_image_header(ota_handle, &app_desc);
    if (err != ESP_OK) {
        snprintf(err_desc, err_desc_size, "Image header verification failed");
        /* OTA should be rejected, returning ESP_ERR_INVALID_STATE */
        err = ESP_ERR_INVALID_STATE;
        goto ota_end;
    }

    /* Report status: Downloading Firmware Image */
    esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_IN_PROGRESS, "Downloading Firmware Image");

    int count = 0;
#ifdef CONFIG_ESP_RMAKER_OTA_PROGRESS_SUPPORT
    int last_ota_progress = 0;
#endif
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err == ESP_ERR_INVALID_VERSION) {
            snprintf(err_desc, err_desc_size, "Chip revision mismatch");
            esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_REJECTED, err_desc);
            /* OTA should be rejected, returning ESP_ERR_INVALID_STATE */
            err = ESP_ERR_INVALID_STATE;
            goto ota_end;
        }
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
#ifdef RMAKER_OTA_HTTP_OTA_RESUMPTION
        /* if file md5 is present, save the written length to NVS */
        if (ota_data->file_md5) {
            /* file md5 is present, only set the written length to NVS */
            if (esp_rmaker_https_ota_set_len_and_md5_to_nvs(esp_https_ota_get_image_len_read(https_ota_handle), NULL) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to save OTA written length to NVS");
            }
        }
#endif
#ifdef CONFIG_ESP_RMAKER_OTA_PROGRESS_SUPPORT
        int image_size = esp_https_ota_get_image_size(https_ota_handle);
        int read_size = esp_https_ota_get_image_len_read(https_ota_handle);
        int ota_progress = 100 * read_size / image_size; // The unit is %
        /* When ota_progress is 0 or 100, we will not report the progress, beacasue the 0 and 100 is reported by additional_info `Downloading Firmware Image` and
         * `Firmware Image download complete`. And every progress will only report once and the progress is increasing.
         */
        if (((ota_progress != 0) && (ota_progress != 100)) && (ota_progress % CONFIG_ESP_RMAKER_OTA_PROGRESS_INTERVAL == 0) && (last_ota_progress < ota_progress)) {
            last_ota_progress = ota_progress;
            char description[40] = {0};
            snprintf(description, sizeof(description), "Downloaded %d%% Firmware Image", ota_progress);
            esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_IN_PROGRESS, description);
        }
#endif
    }
    if (err != ESP_OK) {
        int err_no = errno;
        snprintf(err_desc, err_desc_size, "OTA failed: %s (errno=%d: %s)", esp_err_to_name(err), err_no, err_no ? strerror(err_no) : "Invalid");
        /* OTA failed, may retry later */
        goto ota_end;
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        snprintf(err_desc, err_desc_size, "Complete data was not received");
        /* OTA failed, may retry later */
        err = ESP_FAIL;
        goto ota_end;
    }
#ifdef RMAKER_OTA_HTTP_OTA_RESUMPTION
    if (esp_rmaker_https_ota_cleanup_ota_cfg_from_nvs() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to cleanup OTA config from NVS");
    }
#endif
    /* Report completion before finishing */
    esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_IN_PROGRESS, "Firmware Image download complete");

ota_end:
#ifdef CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI
#if defined(RMAKER_OTA_BT_ENABLED_CHECK)
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
        esp_wifi_set_ps(ps_type);
    }
#else
    esp_wifi_set_ps(ps_type);
#endif /* RMAKER_OTA_BT_ENABLED_CHECK */
#endif /* CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI */

    if (err == ESP_OK) {
        /* Success path: finish the OTA */
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if (ota_finish_err == ESP_OK) {
            return ESP_OK;
        } else if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
            snprintf(err_desc, err_desc_size, "Image validation failed");
        } else {
            int err_no = errno;
            snprintf(err_desc, err_desc_size, "OTA finish failed: %s (errno=%d: %s)", esp_err_to_name(ota_finish_err), err_no, err_no ? strerror(err_no) : "Invalid");
        }
        /* Handle already closed by esp_https_ota_finish(), don't call abort */
        return ESP_FAIL;
    }

    /* Error path: abort the OTA */
    esp_https_ota_abort(https_ota_handle);
    return (err == ESP_ERR_INVALID_STATE) ? ESP_ERR_INVALID_STATE : ESP_FAIL;
}

esp_err_t esp_rmaker_ota_https_cb(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data)
{
    if (!ota_data->url) {
        return ESP_FAIL;
    }

    /* Use the common OTA workflow with HTTPS-specific function */
    return esp_rmaker_ota_start_workflow(ota_handle, ota_data, esp_rmaker_ota_use_https, "HTTPS");
}
