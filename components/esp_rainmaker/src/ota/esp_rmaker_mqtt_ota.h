/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <esp_ota_ops.h>
#include <esp_rmaker_ota.h>

#ifdef __cplusplus
extern "C"
{
#endif
typedef enum {
    FILE_BLOCK_FETCHED = 1,
    FILE_BLOCK_DUPLICATE = 2,
    FILE_FETCH_COMPLETE = 4,
    FILE_BLOCK_FETCH_ERR = 8,
} esp_rmaker_mqtt_events;

typedef enum {
    ESP_MQTT_OTA_INVALID_STATE = -1,
    ESP_MQTT_OTA_INIT,
    ESP_MQTT_OTA_BEGIN,
    ESP_MQTT_OTA_IN_PROGRESS,
    ESP_MQTT_OTA_SUCCESS,
    ESP_MQTT_OTA_FAILED,
} esp_rmaker_mqtt_ota_state;

typedef struct {
    char *stream_id;
    uint32_t filesize;
    uint32_t block_length;
    uint16_t blocks_per_request;
    uint8_t max_retry_count;
} esp_rmaker_mqtt_ota_config_t;

typedef void *esp_rmaker_mqtt_ota_handle_t;

/** Begin MQTT OTA Update
 *
 * This API allocates memory to the buffer and initializes the initial OTA state.
 *
 * @param[in] config MQTT OTA Configuration
 * @param[out] handle MQTT OTA handle required by other MQTT OTA APIs
 *
 * @return ESP_OK if the MQTT OTA handle is created.
 * @return error on failure
 */
esp_err_t esp_mqtt_ota_begin(esp_rmaker_mqtt_ota_config_t *config, esp_rmaker_mqtt_ota_handle_t *handle);

/** Set progress callback for MQTT OTA Update
 *
 * This API sets a callback function that will be called during OTA progress
 * to report the number of bytes downloaded and total file size.
 *
 * @param[in] mqtt_ota_handle MQTT OTA handle
 * @param[in] progress_cb Progress callback function
 * @param[in] priv Private data passed to callback function
 *
 * @return ESP_OK on success.
 * @return error on failure
 */
esp_err_t esp_mqtt_ota_set_progress_cb(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle, void (*progress_cb)(int bytes_read, int total_bytes, void *priv), void *priv);

/** Perform MQTT OTA Update file fetch operations.
 *
 * This API will fetch blocks of the OTA image as specified in the configuration.
 * This API should be called repeatedly until all blocks are fetched or a failure occurs.
 *
 * @param[in] mqtt_handle MQTT OTA handle
 *
 * @return ESP_OK if the file fetch request was successful.
 * @return error on failure
 */
esp_err_t esp_mqtt_ota_perform(esp_rmaker_mqtt_ota_handle_t mqtt_handle);

/** Free the memory resources allocated for MQTT OTA and update the device boot partition.
 *
 * This API should be called after the OTA image was fetched successfully.
 *
 * @param[in] mqtt_handle MQTT OTA handle
 *
 * @return ESP_OK on success.
 * @return error on failure
 */
esp_err_t esp_mqtt_ota_finish(esp_rmaker_mqtt_ota_handle_t mqtt_handle);

/** Free the memory resources allocated for MQTT OTA.
 *
 * This API should be called to cancel the OTA update.
 *
 * @param[in] mqtt_ota_handle MQTT OTA handle
 *
 * @return ESP_OK on success.
 * @return error on failure
 */
esp_err_t esp_mqtt_ota_abort(esp_rmaker_mqtt_ota_handle_t mqtt_handle);

/** Get current OTA state
 *
 * @param[in] mqtt_ota_handle MQTT OTA handle
 *
 * @return state of MQTT OTA
 * @return ESP_MQTT_OTA_INVALID_STATE if mqtt_ota_handle is NULL
 */
esp_rmaker_mqtt_ota_state esp_mqtt_ota_get_state(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle);

/** Get downloaded length of OTA image
 *
 * @param[in] mqtt_ota_handle MQTT OTA handle
 *
 * @return length of downloaded image
 */
int esp_mqtt_ota_get_image_len_read(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle);

/** Find whether OTA image download is complete.
 *
 * @param[in] mqtt_ota_handle MQTT OTA handle
 *
 * @return true if complete OTA image is downloaded; false otherwise.
 */
bool esp_mqtt_ota_is_complete_data_received(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle);

/** Read the OTA image header and retrieve its app description.
 *
 * @param[in] mqtt_ota_handle MQTT OTA handle
 * @param[out] new_app_info Image description
 *
 * @return ESP_OK if the image description was fetched successfully.
 * @return error on failure
 */
esp_err_t esp_mqtt_ota_get_img_desc(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle, esp_app_desc_t *new_app_info);

/**
 * @brief MQTT OTA callback function
 *
 * This function handles MQTT OTA update with retry logic and status reporting.
 * It orchestrates the entire MQTT OTA process from start to finish.
 *
 * @param[in] ota_handle The OTA handle
 * @param[in] ota_data The OTA data containing stream_id, filesize and other information
 *
 * @return ESP_OK on success
 * @return ESP_FAIL on failure
 */
esp_err_t esp_rmaker_ota_mqtt_cb(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data);

#ifdef __cplusplus
}
#endif
