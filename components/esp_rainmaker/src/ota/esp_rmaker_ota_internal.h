/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once


#include <stdint.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <esp_rmaker_ota.h>
#include <esp_app_format.h>
#include <esp_ota_ops.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RMAKER_OTA_NVS_NAMESPACE            "rmaker_ota"
#define RMAKER_OTA_JOB_ID_NVS_NAME          "rmaker_ota_id"
#define RMAKER_OTA_UPDATE_FLAG_NVS_NAME     "ota_update"
#define RMAKER_OTA_FETCH_DELAY              5

#if defined(CONFIG_BT_ENABLED) && !defined(CONFIG_IDF_TARGET_ESP32P4)
/* For checking if BT enabled on the device and disable Wi-Fi PS */
#define RMAKER_OTA_BT_ENABLED_CHECK 1
#include <esp_bt.h>
#endif /* CONFIG_BT_ENABLED && !CONFIG_IDF_TARGET_ESP32P4 */

typedef struct {
    esp_rmaker_ota_type_t type;
    esp_rmaker_ota_cb_t ota_cb;
    void *priv;
    esp_rmaker_post_ota_diag_t ota_diag;
    TimerHandle_t rollback_timer;
    const char *server_cert;
    char *url;
    char *fw_version;
    char *file_md5;
#ifdef CONFIG_ESP_RMAKER_OTA_USE_MQTT
    char *stream_id;
#endif
    int filesize;
    bool ota_in_progress;
    bool validation_in_progress;
    bool rolled_back;
    ota_status_t last_reported_status;
    void *transient_priv;
    char *metadata;
} esp_rmaker_ota_t;



char *esp_rmaker_ota_status_to_string(ota_status_t status);
esp_err_t esp_rmaker_ota_post_event(esp_rmaker_event_t event_id, void *data, size_t data_size);
typedef enum {
    OTA_OK = 0,
    OTA_ERR,
    OTA_DELAYED
} esp_rmaker_ota_action_t;

void esp_rmaker_ota_common_cb(void *priv);
void esp_rmaker_ota_finish_using_params(esp_rmaker_ota_t *ota);
void esp_rmaker_ota_finish_using_topics(esp_rmaker_ota_t *ota);
esp_err_t esp_rmaker_ota_enable_using_params(esp_rmaker_ota_t *ota);
esp_err_t esp_rmaker_ota_report_status_using_params(esp_rmaker_ota_handle_t ota_handle,
            ota_status_t status, char *additional_info);
esp_err_t esp_rmaker_ota_enable_using_topics(esp_rmaker_ota_t *ota);
esp_err_t esp_rmaker_ota_report_status_using_topics(esp_rmaker_ota_handle_t ota_handle,
        ota_status_t status, char *additional_info);

/* Common retry loop infrastructure with function pointer pattern */
typedef esp_err_t (*ota_protocol_func_t)(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data, char *err_desc, size_t err_desc_size);

/* Complete OTA workflow orchestration */
esp_err_t esp_rmaker_ota_start_workflow(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data,
                                       ota_protocol_func_t protocol_func, const char *protocol_name);

/* OTA image header validation */
esp_err_t validate_image_header(esp_rmaker_ota_handle_t ota_handle, esp_app_desc_t *new_app_info);

#ifdef __cplusplus
}
#endif
