/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * AWS IoT Over-the-air Update v3.4.0
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <esp_err.h>
#include <esp_log.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_timer.h>
#include <mbedtls/base64.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_mqtt.h>
#include <esp_rmaker_mqtt_ota.h>
#include <esp_rmaker_utils.h>

/* Additional includes for high-level MQTT OTA functions */
#include <esp_rmaker_ota.h>
#ifdef CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI
#include <esp_wifi.h>
#endif

#include "esp_rmaker_ota_internal.h"

#ifndef MIN
#define MAX(a, b) ({            \
    __typeof__ (a) _a = (a);    \
    __typeof__ (b) _b = (b);    \
    _a > _b ? _a : _b;          \
})

#define MIN(a, b) ({            \
    __typeof__ (a) _a = (a);    \
    __typeof__ (b) _b = (b);    \
    _a < _b ? _a : _b;          \
})
#endif

#define WAIT_FOR_DATA_SEC           10
#define LOG2_BITS_PER_BYTE          3U
#define BITS_PER_BYTE               ((uint32_t)1U << LOG2_BITS_PER_BYTE)
#define OTA_ERASED_BLOCKS_VAL       0xffU
#define IMAGE_HEADER_SIZE           sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t) + 1

// AWS MQTT File Delivery does not allow to fetch more than 128kb of data in a single request
#define MQTT_OTA_MAX_BYTES_PER_REQUEST (128*1024)
    #if (CONFIG_ESP_RMAKER_MQTT_OTA_NO_OF_BLOCKS * CONFIG_ESP_RMAKER_MQTT_OTA_BLOCK_SIZE) > MQTT_OTA_MAX_BYTES_PER_REQUEST
        #error Total block size per request should not exceed 128kb
    #endif

static const char *TAG = "esp_rmaker_mqtt_ota";

/* Add global counter for progress reporting */
static int mqtt_ota_block_count = 0;

typedef struct {
    int current_offset;
    int remaining_size;
    int current_block_length;
    int current_no_of_blocks;
    uint32_t blocks_remaining;       /*!< @brief How many blocks remain to be received (a code optimization). */
    uint8_t *block_bitmap;       /*!< @brief Bitmap to track of received blocks. */
    int32_t bitmap_len;
    int file_id;
    int stream_version;
    int bytes_read;
} esp_rmaker_mqtt_file_params_t;

typedef struct {
    esp_ota_handle_t update_handle;
    const esp_partition_t *update_partition;
    char *stream_id;
    uint8_t *ota_upgrade_buf;
    size_t ota_upgrade_buf_size;
    int binary_file_len;
    int image_length;
    esp_rmaker_mqtt_ota_state state;
    char *image_header_buf;
    esp_rmaker_mqtt_file_params_t *file_fetch_params;
    uint8_t retry_count;
    uint8_t max_retries;
    /* Progress reporting fields */
    void (*progress_cb)(int bytes_read, int total_bytes, void *priv);
    void *progress_priv;
    int last_reported_progress;
} esp_rmaker_mqtt_ota_t;

typedef struct {
    uint64_t stream_version;
    uint8_t file_id;
    uint32_t length;
    uint32_t offset;
    uint32_t no_of_blocks;
    uint8_t *block_bitmap;
    int32_t bitmap_len;
} get_stream_req_t;

typedef struct {
    uint8_t *payload;
    size_t payload_len;
    int32_t file_id;
    int32_t block_number;
    int32_t length;
} get_stream_res_t;

static EventGroupHandle_t mqtt_ota_event_group;

/* MQTT OTA uses CBOR encoding for optimized size and performance */
#include "cbor.h"
#define MQTT_FILE_DELIVERY_TOPIC_SUFFIX "cbor"

static CborError cbor_check_data_type(CborType expected_type, const CborValue *cbor_value)
{
    CborType actual_type = cbor_value_get_type(cbor_value);
    return (actual_type != expected_type) ? CborErrorIllegalType : CborNoError;
}

static esp_err_t decode_cbor_message(const uint8_t *message_buf, size_t message_size,
                                      int32_t *file_id, int32_t *block_id, int32_t *block_size,
                                      uint8_t *const *payload, size_t *payload_size)
{
    CborParser cbor_parser;
    CborValue cbor_value, cborMap;
    size_t payload_size_received = 0;

    if ((file_id == NULL) || (block_id == NULL) || (block_size == NULL) ||
            (payload == NULL) || (payload_size == NULL) || (message_buf == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Initialize the parser. */
    if (cbor_parser_init(message_buf, message_size, 0, &cbor_parser, &cborMap) != CborNoError) {
        return ESP_FAIL;
    }

    /* Get the outer element and confirm that it's a "map," i.e., a set of
     * CBOR key/value pairs. */
    if (cbor_value_is_map(&cborMap) == false) {
        return ESP_FAIL;
    }

    /* Find the file ID. */
    if (cbor_value_map_find_value(&cborMap, "f", &cbor_value) != CborNoError) {
        return ESP_FAIL;
    }
    if (cbor_check_data_type(CborIntegerType, &cbor_value) != CborNoError) {
        return ESP_FAIL;
    }
    if (cbor_value_get_int(&cbor_value, (int *)file_id) != CborNoError) {
        return ESP_FAIL;
    }

    /* Find the block ID. */
    if (cbor_value_map_find_value(&cborMap, "i", &cbor_value) != CborNoError) {
        return ESP_FAIL;
    }
    if (cbor_check_data_type(CborIntegerType, &cbor_value) != CborNoError) {
        return ESP_FAIL;
    }
    if (cbor_value_get_int(&cbor_value, (int *)block_id) != CborNoError) {
        return ESP_FAIL;
    }

    /* Find the block size. */
    if (cbor_value_map_find_value(&cborMap, "l", &cbor_value) != CborNoError) {
        return ESP_FAIL;
    }
    if (cbor_check_data_type(CborIntegerType, &cbor_value) != CborNoError) {
        return ESP_FAIL;
    }
    if (cbor_value_get_int(&cbor_value, (int *)block_size) != CborNoError) {
        return ESP_FAIL;
    }

    /* Find the payload bytes. */
    if (cbor_value_map_find_value(&cborMap, "p", &cbor_value) != CborNoError) {
        return ESP_FAIL;
    }
    if (cbor_check_data_type(CborByteStringType, &cbor_value) != CborNoError) {
        return ESP_FAIL;
    }

    /* Calculate the size we need to malloc for the payload. */
    if (cbor_value_calculate_string_length(&cbor_value, &payload_size_received) != CborNoError) {
        return ESP_FAIL;
    }

    /* Check if the received payload size is less than or equal to buffer size. */
    if (payload_size_received <= (*payload_size)) {
        *payload_size = payload_size_received;
    } else {
        return ESP_FAIL;
    }

    if (cbor_value_copy_byte_string(&cbor_value, *payload, payload_size, NULL) != CborNoError) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t encode_cbor_message(uint8_t *message_buf, size_t msg_buf_size, size_t *encoded_msg_size,
                                      int32_t file_id, int32_t block_size, int32_t block_offset,
                                      const uint8_t *block_bitmap, size_t block_bitmap_size, int32_t num_of_blocks_requested)
{
    CborEncoder cbor_encoder, cbor_map_encoder;

    if ((message_buf == NULL) || (encoded_msg_size == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t item_count = 5;
    if (block_bitmap == NULL) {
        item_count = 4;
    }

    /* Initialize the CBOR encoder. */
    cbor_encoder_init(&cbor_encoder, message_buf, msg_buf_size, 0);
    if (cbor_encoder_create_map(&cbor_encoder, &cbor_map_encoder, item_count) != CborNoError) {
        return ESP_FAIL;
    }

    /* Encode the file ID key and value. */
    if (cbor_encode_text_stringz(&cbor_map_encoder, "f") != CborNoError) {
        return ESP_FAIL;
    }
    if (cbor_encode_int(&cbor_map_encoder, file_id) != CborNoError) {
        return ESP_FAIL;
    }

    /* Encode the block size key and value. */
    if (cbor_encode_text_stringz(&cbor_map_encoder, "l") != CborNoError) {
        return ESP_FAIL;
    }
    if (cbor_encode_int(&cbor_map_encoder, block_size) != CborNoError) {
        return ESP_FAIL;
    }

    /* Encode the block offset key and value. */
    if (cbor_encode_text_stringz(&cbor_map_encoder, "o") != CborNoError) {
        return ESP_FAIL;
    }
    if (cbor_encode_int(&cbor_map_encoder, block_offset) != CborNoError) {
        return ESP_FAIL;
    }

    /* Encode the block bitmap key and value. */
    if (block_bitmap != NULL) {
        if (cbor_encode_text_stringz(&cbor_map_encoder, "b") != CborNoError) {
            return ESP_FAIL;
        }
        if (cbor_encode_byte_string(&cbor_map_encoder, block_bitmap, block_bitmap_size) != CborNoError) {
            return ESP_FAIL;
        }
    }

    /* Encode the number of blocks requested key and value. */
    if (cbor_encode_text_stringz(&cbor_map_encoder, "n") != CborNoError) {
        return ESP_FAIL;
    }
    if (cbor_encode_int(&cbor_map_encoder, num_of_blocks_requested) != CborNoError) {
        return ESP_FAIL;
    }

    /* Close the encoder. */
    if (cbor_encoder_close_container_checked(&cbor_encoder, &cbor_map_encoder) != CborNoError) {
        return ESP_FAIL;
    }

    /* Get the encoded size. */
    *encoded_msg_size = cbor_encoder_get_buffer_size(&cbor_encoder, message_buf);

    return ESP_OK;
}

static esp_err_t create_get_stream_data_request(uint8_t *buf, size_t len, size_t *encoded_size, const get_stream_req_t *req)
{
    if (!buf || !encoded_size || !req || (len <= 0)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = encode_cbor_message(buf, len, encoded_size, req->file_id, req->length, req->offset,
                                        (uint8_t *)req->block_bitmap, req->bitmap_len, req->no_of_blocks);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in encoding cbor message.");
    }
    return ret;
}

static esp_err_t create_get_stream_data_response(uint8_t *buf, size_t len, get_stream_res_t *res, esp_rmaker_mqtt_ota_t *handle)
{
    if (!buf || (len <= 0) || !res || !handle) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = decode_cbor_message(buf, len, &res->file_id, &res->block_number,
                                        &res->length, &(res->payload), &res->payload_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in decoding cbor response.");
    }
    return ret;
}



static bool _received_complete_response(esp_rmaker_mqtt_ota_t *handle, const get_stream_req_t *req)
{
    for (int i = req->offset; i < (req->offset + req->no_of_blocks); i++) {
        int32_t byte = i >> LOG2_BITS_PER_BYTE;
        // Check if block received is duplicate
        if (((handle->file_fetch_params->block_bitmap[ byte ] >> (i % BITS_PER_BYTE)) & (int8_t)0x01U) != 0) {
            return false;
        }
    }
    return true;
}

static esp_err_t _ota_write(esp_rmaker_mqtt_ota_t *mqtt_ota_handle, const void *buffer, size_t buf_len, size_t offset)
{
    if (buffer == NULL || mqtt_ota_handle == NULL || (buf_len <= 0)) {
        ESP_LOGE(TAG, "_ota_write: Invalid arguments.");
        return ESP_FAIL;
    }
    esp_err_t err = esp_ota_write_with_offset(mqtt_ota_handle->update_handle, buffer, buf_len, offset);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", err);
    } else {
        mqtt_ota_handle->binary_file_len += buf_len;
    }
    return err;
}

static void error_cb(const char *topic, void *payload, size_t payload_len, void *priv_data)
{
    char *output = payload;
    ESP_LOGE(TAG, "Received error response on topic : %s", topic);
    ESP_LOGD(TAG, "Data received : %.*s", payload_len, output);
    ESP_LOGD(TAG, "Length of received data : %d", payload_len);
    xEventGroupSetBits(mqtt_ota_event_group, FILE_BLOCK_FETCH_ERR);
}

static void stream_data_cb(const char *topic, void *payload, size_t payload_len, void *priv_data)
{
    esp_rmaker_mqtt_ota_t *handle = (esp_rmaker_mqtt_ota_t *)priv_data;
    if (!handle || (handle->state > ESP_MQTT_OTA_IN_PROGRESS) || (handle->state < ESP_MQTT_OTA_BEGIN)) {
        return;
    }
    esp_rmaker_mqtt_file_params_t *file_fetch_params = handle->file_fetch_params;
    if (!file_fetch_params) {
        return;
    }
    get_stream_res_t response_data;
    int32_t byte;
    int8_t bit_mask;
    response_data.payload = handle->ota_upgrade_buf;
    response_data.payload_len = file_fetch_params->current_block_length;
    esp_err_t err = create_get_stream_data_response(payload, payload_len, &response_data, handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to decode response.");
        return;
    }
    // check duplicate blocks when bitmap is not used
    if (response_data.block_number < handle->file_fetch_params->current_offset ||
            response_data.block_number >= handle->file_fetch_params->current_offset + handle->file_fetch_params->current_no_of_blocks) {
        ESP_LOGD(TAG, "Received duplicate block. Discarding...");
        return;
    }
    switch (handle->state) {
        case ESP_MQTT_OTA_BEGIN:
            if (response_data.block_number == 0 && response_data.payload_len >= IMAGE_HEADER_SIZE && handle->image_header_buf != NULL) {
                memcpy(handle->image_header_buf, handle->ota_upgrade_buf, IMAGE_HEADER_SIZE);
            }
            xEventGroupSetBits(mqtt_ota_event_group, FILE_BLOCK_FETCHED);
            return;
        case ESP_MQTT_OTA_IN_PROGRESS:
            byte = response_data.block_number >> LOG2_BITS_PER_BYTE;
            bit_mask = (uint8_t)(1U << (response_data.block_number % BITS_PER_BYTE));
            // Check if block received is duplicate
            if (((file_fetch_params->block_bitmap[ byte ] >> (response_data.block_number % BITS_PER_BYTE)) & (int8_t)0x01U) == 0) {
                ESP_LOGI(TAG, "Duplicate Block Received.");
                xEventGroupSetBits(mqtt_ota_event_group, FILE_BLOCK_DUPLICATE);
                return;
            }
            // Update Bitmap
            file_fetch_params->block_bitmap[byte] &= (uint8_t)((uint8_t) 0xFFU & (~bit_mask));

            file_fetch_params->blocks_remaining -= 1;
            file_fetch_params->bytes_read += response_data.payload_len;
            file_fetch_params->remaining_size -= response_data.payload_len;

            if (_ota_write(handle, handle->ota_upgrade_buf, response_data.payload_len,
                    response_data.block_number * file_fetch_params->current_block_length) == ESP_OK) {

                /* Call progress callback if set */
                if (handle->progress_cb) {
                    handle->progress_cb(handle->binary_file_len, handle->image_length, handle->progress_priv);
                }

                /* Log progress occasionally for debugging */
                mqtt_ota_block_count++;
                if (mqtt_ota_block_count % 20 == 0) {
                    ESP_LOGI(TAG, "Image bytes read: %d", handle->binary_file_len);
                }

                xEventGroupSetBits(mqtt_ota_event_group, FILE_BLOCK_FETCHED);
            } else {
                xEventGroupSetBits(mqtt_ota_event_group, FILE_BLOCK_FETCH_ERR);
            }
            return;
        default:
            ESP_LOGE(TAG, "Invalid OTA State: %d. Discarding block...", handle->state);
    }
}

static esp_err_t esp_rmaker_fetch_block(esp_rmaker_mqtt_ota_t *handle, get_stream_req_t *req)
{
    char publish_topic[100];
    snprintf(publish_topic, sizeof(publish_topic), "$aws/things/%s/streams/%s/get/%s",
             esp_rmaker_get_node_id(), handle->stream_id, MQTT_FILE_DELIVERY_TOPIC_SUFFIX);
    uint8_t publish_payload[200];
    size_t encoded_size = 0;
    esp_err_t err = create_get_stream_data_request(publish_payload, sizeof(publish_payload), &encoded_size, req);
    if (err != ESP_OK) {
        return ESP_FAIL;
    }
    err = esp_rmaker_mqtt_publish(publish_topic, publish_payload, encoded_size, RMAKER_MQTT_QOS1, NULL);
    return err;
}

static esp_err_t esp_rmaker_mqtt_fetch_file(esp_rmaker_mqtt_ota_t *handle)
{
    EventBits_t uxBits;
    esp_rmaker_mqtt_file_params_t *file_fetch_params = handle->file_fetch_params;
    esp_err_t err;
    int blocks_received = 0;
    get_stream_req_t req = {
        .stream_version = file_fetch_params->stream_version,
        .file_id = file_fetch_params->file_id,
        .offset = file_fetch_params->current_offset,
        .length = file_fetch_params->current_block_length,
        .no_of_blocks = file_fetch_params->current_no_of_blocks,
        .block_bitmap = NULL, // To pass bitmap with the request, pass pointer to bitmap.
        .bitmap_len = file_fetch_params->bitmap_len
    };
    err = esp_rmaker_fetch_block(handle, &req);
    if (err != ESP_OK) {
        return err;
    }
    while (blocks_received < file_fetch_params->current_no_of_blocks) {
        uxBits = xEventGroupWaitBits(mqtt_ota_event_group, FILE_BLOCK_FETCHED | FILE_BLOCK_FETCH_ERR | FILE_BLOCK_DUPLICATE,
                                     pdTRUE, pdFALSE, (WAIT_FOR_DATA_SEC * 1000) / portTICK_PERIOD_MS);
        if ((uxBits & FILE_BLOCK_FETCHED) != 0) {
            blocks_received++;
            handle->retry_count = 0;
        } else if ((uxBits & FILE_BLOCK_DUPLICATE) != 0) {
            blocks_received++;
        } else if ((uxBits & FILE_BLOCK_FETCH_ERR) != 0) {
            err = ESP_FAIL;
            break;
        } else { // Timeout
            ESP_LOGE(TAG, "Request timed out.");
            if (handle->retry_count < handle->max_retries) {
                handle->retry_count += 1;
                err = ESP_OK;
                break;
            } else {
                ESP_LOGE(TAG, "Out of retries. Aborting OTA...");
                err = ESP_ERR_TIMEOUT;
                break;
            }
        }
    }
    if (err == ESP_OK) {
        if (_received_complete_response(handle, &req) == true) {
            file_fetch_params->current_offset += file_fetch_params->current_no_of_blocks;
            file_fetch_params->current_no_of_blocks =
                MIN(handle->file_fetch_params->blocks_remaining, file_fetch_params->current_no_of_blocks);
        }
    }
    return err;
}


static esp_err_t esp_rmaker_mqtt_subscribe_to_stream_topics(esp_rmaker_mqtt_ota_t *handle)
{
    esp_err_t err;
    char *node_id = esp_rmaker_get_node_id();
    char subscribe_topic[100];
    snprintf(subscribe_topic, sizeof(subscribe_topic), "$aws/things/%s/streams/%s/rejected/%s",
             node_id, handle->stream_id, MQTT_FILE_DELIVERY_TOPIC_SUFFIX);
    if ((err = esp_rmaker_mqtt_subscribe(subscribe_topic, error_cb, RMAKER_MQTT_QOS1, handle->file_fetch_params)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to subscribe to %s", subscribe_topic);
    }
    snprintf(subscribe_topic, sizeof(subscribe_topic), "$aws/things/%s/streams/%s/data/%s",
             node_id, handle->stream_id, MQTT_FILE_DELIVERY_TOPIC_SUFFIX);
    if ((err = esp_rmaker_mqtt_subscribe(subscribe_topic, stream_data_cb, RMAKER_MQTT_QOS1, handle)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to subscribe to %s", subscribe_topic);
    }
    return err;
}

static esp_err_t read_header(esp_rmaker_mqtt_ota_t *handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->image_header_buf) {
        return ESP_OK;
    }
    get_stream_req_t req = {
        .stream_version = 1,
        .offset = 0,
        .no_of_blocks = 1,
        .length = IMAGE_HEADER_SIZE,
        .file_id = handle->file_fetch_params->file_id,
        .block_bitmap = NULL,
        .bitmap_len = 0
    };
    handle->image_header_buf = MEM_CALLOC_EXTRAM(1, IMAGE_HEADER_SIZE);
    if (!handle->image_header_buf) {
        ESP_LOGE(TAG, "Failed to allocate memory to image header data buffer");
        return ESP_ERR_NO_MEM;
    }
    if (esp_rmaker_fetch_block(handle, &req) != ESP_OK) {
        return ESP_FAIL;
    }
    EventBits_t uxBits = xEventGroupWaitBits(mqtt_ota_event_group, FILE_BLOCK_FETCHED | FILE_BLOCK_FETCH_ERR,
                                             pdTRUE, pdFALSE, (WAIT_FOR_DATA_SEC * 1000) / portTICK_PERIOD_MS);
    if ((uxBits & FILE_BLOCK_FETCHED) != 0) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

static esp_err_t esp_ota_verify_chip_id(void *arg)
{
    esp_image_header_t *data = (esp_image_header_t*)(arg);
    if (data->chip_id != CONFIG_IDF_FIRMWARE_CHIP_ID) {
        ESP_LOGE(TAG, "Mismatch chip id, expected %d, found %d", CONFIG_IDF_FIRMWARE_CHIP_ID, data->chip_id);
        return ESP_ERR_INVALID_VERSION;
    }
    return ESP_OK;
}

static esp_err_t esp_rmaker_mqtt_unsubscribe_stream_topics(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle)
{
    esp_rmaker_mqtt_ota_t *handle = (esp_rmaker_mqtt_ota_t *)mqtt_ota_handle;
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    char topic[100] = {0};
    char *node_id = esp_rmaker_get_node_id();
    esp_err_t err = ESP_OK;
    snprintf(topic, sizeof(topic), "$aws/things/%s/streams/%s/rejected/%s",
             node_id, handle->stream_id, MQTT_FILE_DELIVERY_TOPIC_SUFFIX);
    if ((err = esp_rmaker_mqtt_unsubscribe(topic)) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to unsubscribe from %s", topic);
    }
    snprintf(topic, sizeof(topic), "$aws/things/%s/streams/%s/data/%s",
             node_id, handle->stream_id, MQTT_FILE_DELIVERY_TOPIC_SUFFIX);
    if ((err = esp_rmaker_mqtt_unsubscribe(topic)) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to unsubscribe from %s", topic);
    }
    return err;
}


esp_err_t esp_mqtt_ota_begin(esp_rmaker_mqtt_ota_config_t *config, esp_rmaker_mqtt_ota_handle_t *handle)
{
    esp_err_t err;
    /* Create event group only if it doesn't exist to prevent memory leak */
    if (mqtt_ota_event_group == NULL) {
        mqtt_ota_event_group = xEventGroupCreate();
        if (mqtt_ota_event_group == NULL) {
            ESP_LOGE(TAG, "Failed to create MQTT OTA event group");
            return ESP_ERR_NO_MEM;
        }
    }
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Reset progress counter for new OTA session */
    mqtt_ota_block_count = 0;

    esp_rmaker_mqtt_ota_t *mqtt_ota_handle = MEM_CALLOC_EXTRAM(1, sizeof(esp_rmaker_mqtt_ota_t));

    if (!mqtt_ota_handle) {
        ESP_LOGE(TAG, "Failed to allocate memory to ota handle");
        *handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    mqtt_ota_handle->file_fetch_params = MEM_CALLOC_EXTRAM(1, sizeof(esp_rmaker_mqtt_file_params_t));
    if (!mqtt_ota_handle->file_fetch_params) {
        ESP_LOGE(TAG, "Failed to allocate memory to file fetch parameters");
        err = ESP_ERR_NO_MEM;
        goto mqtt_cleanup;
    }

    mqtt_ota_handle->stream_id = config->stream_id;
    err = esp_rmaker_mqtt_subscribe_to_stream_topics(mqtt_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to subscribe to topics");
        goto mqtt_cleanup;
    }

    mqtt_ota_handle->image_length = (int)config->filesize;
    mqtt_ota_handle->max_retries = config->max_retry_count;

    /* Initialize progress reporting fields */
    mqtt_ota_handle->progress_cb = NULL;
    mqtt_ota_handle->progress_priv = NULL;
    mqtt_ota_handle->last_reported_progress = 0;

    ESP_LOGI(TAG, "Image length = %d bytes.", mqtt_ota_handle->image_length);
    mqtt_ota_handle->update_partition = NULL;
    ESP_LOGI(TAG, "Starting OTA...");

    mqtt_ota_handle->update_partition = esp_ota_get_next_update_partition(NULL);
    if (mqtt_ota_handle->update_partition == NULL) {
        ESP_LOGE(TAG, "Passive OTA partition not found.");
        err = ESP_FAIL;
        goto mqtt_cleanup;
    }

    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%"PRIx32,
        mqtt_ota_handle->update_partition->subtype, mqtt_ota_handle->update_partition->address);

    const int alloc_size = MAX(config->block_length, IMAGE_HEADER_SIZE);
    mqtt_ota_handle->ota_upgrade_buf = (uint8_t *)MEM_ALLOC_EXTRAM(alloc_size);
    if (!mqtt_ota_handle->ota_upgrade_buf) {
        ESP_LOGE(TAG, "Failed to allocate memory to OTA upgrade data buffer.");
        err = ESP_ERR_NO_MEM;
        goto mqtt_cleanup;
    }
    mqtt_ota_handle->image_header_buf = NULL;
    mqtt_ota_handle->ota_upgrade_buf_size = alloc_size;
    mqtt_ota_handle->binary_file_len = 0;

    mqtt_ota_handle->file_fetch_params->stream_version = 1;
    mqtt_ota_handle->file_fetch_params->file_id = 1;
    mqtt_ota_handle->file_fetch_params->current_block_length = config->block_length;
    mqtt_ota_handle->file_fetch_params->current_offset = 0;
    mqtt_ota_handle->file_fetch_params->current_no_of_blocks = config->blocks_per_request;
    mqtt_ota_handle->file_fetch_params->remaining_size = mqtt_ota_handle->image_length;


    uint8_t *block_bitmap = NULL;
    uint32_t file_size = mqtt_ota_handle->image_length;
    uint32_t num_blocks = (file_size / config->block_length) + ((file_size % config->block_length  > 0) ? 1 : 0);
    uint32_t bitmap_len = (num_blocks + (BITS_PER_BYTE - 1)) >> LOG2_BITS_PER_BYTE;
    uint8_t bit = 1U << (BITS_PER_BYTE - 1U);
    uint32_t num_out_of_range = (bitmap_len * BITS_PER_BYTE) - num_blocks;

    block_bitmap = MEM_ALLOC_EXTRAM(bitmap_len);
    if (!block_bitmap) {
        ESP_LOGE(TAG, "Failed to allocate memory to bitmap.");
        err = ESP_ERR_NO_MEM;
        goto mqtt_cleanup;
    }
    /* Set all bits in the bitmap to the erased state (we use 1 for erased just like flash memory). */
    memset(block_bitmap, (int32_t)OTA_ERASED_BLOCKS_VAL, bitmap_len);

    uint32_t index;
    for (index = 0; index < num_out_of_range; index++) {
        block_bitmap[bitmap_len - 1] &= (uint8_t)((uint8_t)0xFFU & (~bit));
        bit >>= 1U;
    }
    mqtt_ota_handle->file_fetch_params->block_bitmap = block_bitmap;
    mqtt_ota_handle->file_fetch_params->bitmap_len = bitmap_len;
    mqtt_ota_handle->file_fetch_params->blocks_remaining = num_blocks;
    *handle = (esp_rmaker_mqtt_ota_handle_t)mqtt_ota_handle;
    mqtt_ota_handle->state = ESP_MQTT_OTA_BEGIN;
    return ESP_OK;

mqtt_cleanup:
    if (mqtt_ota_handle) {
        if (mqtt_ota_handle->file_fetch_params) {
            free(mqtt_ota_handle->file_fetch_params);
            mqtt_ota_handle->file_fetch_params = NULL;
        }
        if (mqtt_ota_handle->ota_upgrade_buf) {
            free(mqtt_ota_handle->ota_upgrade_buf);
            mqtt_ota_handle->ota_upgrade_buf = NULL;
        }
        free(mqtt_ota_handle);
        mqtt_ota_handle = NULL;
    }
    *handle = NULL;
    return err;
}

esp_err_t esp_mqtt_ota_perform(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle)
{
    esp_rmaker_mqtt_ota_t *handle = (esp_rmaker_mqtt_ota_t *)mqtt_ota_handle;
    if (handle == NULL) {
        ESP_LOGE(TAG, "esp_mqtt_ota_perform: Invalid argument.");
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->state < ESP_MQTT_OTA_BEGIN) {
        ESP_LOGE(TAG, "esp_mqtt_ota_perform: Invalid state.");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = ESP_OK;
    switch(handle->state) {
        case ESP_MQTT_OTA_BEGIN:
            err = esp_ota_begin(handle->update_partition, handle->image_length, &handle->update_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                handle->state = ESP_MQTT_OTA_FAILED;
                return err;
            }
            if (read_header(handle) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to read header");
                handle->state = ESP_MQTT_OTA_FAILED;
                return ESP_FAIL;
            }
            handle->binary_file_len = 0;
            err = esp_ota_verify_chip_id(handle->image_header_buf);
            if (handle->image_header_buf) {
                free(handle->image_header_buf);
                handle->image_header_buf = NULL;
            }
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to verify chip ID.");
                handle->state = ESP_MQTT_OTA_FAILED;
                return err;
            }
            handle->file_fetch_params->bytes_read = 0;
            handle->state = ESP_MQTT_OTA_IN_PROGRESS;
            return err;
        case ESP_MQTT_OTA_IN_PROGRESS:
            err = esp_rmaker_mqtt_fetch_file(handle);
            if (err != ESP_OK) {
                handle->state = ESP_MQTT_OTA_FAILED;
                return ESP_FAIL;
            }
            if (handle->file_fetch_params->bytes_read > 0) {
                handle->file_fetch_params->bytes_read = 0;
                if (handle->file_fetch_params->blocks_remaining == 0) {
                    ESP_LOGI(TAG, "Firmware Image fetched successfully.");
                }
            }
            if (handle->image_length == handle->binary_file_len) {
                handle->state = ESP_MQTT_OTA_SUCCESS;
            }
            break;
        case ESP_MQTT_OTA_SUCCESS:
            break;
        case ESP_MQTT_OTA_FAILED:
            break;
        default:
            return ESP_FAIL;
            break;
    }
    return ESP_OK;
}

static esp_err_t esp_mqtt_ota_end(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle, bool set_boot_partition)
{
    esp_rmaker_mqtt_ota_t *handle = (esp_rmaker_mqtt_ota_t *)mqtt_ota_handle;
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->state < ESP_MQTT_OTA_BEGIN) {
        return ESP_FAIL;
    }
    esp_err_t err = ESP_OK;
    esp_rmaker_mqtt_unsubscribe_stream_topics(mqtt_ota_handle);
    switch(handle->state) {
        case ESP_MQTT_OTA_FAILED:
            /* fall through */
        case ESP_MQTT_OTA_SUCCESS:
            /* fall through */
        case ESP_MQTT_OTA_IN_PROGRESS:
            err = esp_ota_end(handle->update_handle);
            /* fall through */
        case ESP_MQTT_OTA_BEGIN:
            if (handle->ota_upgrade_buf) {
                free(handle->ota_upgrade_buf);
                handle->ota_upgrade_buf = NULL;
            }
            if (handle->file_fetch_params) {
                if (handle->file_fetch_params->block_bitmap) {
                    free(handle->file_fetch_params->block_bitmap);
                    handle->file_fetch_params->block_bitmap = NULL;
                }
                free(handle->file_fetch_params);
                handle->file_fetch_params = NULL;
            }
            if (handle->image_header_buf) {
                free(handle->image_header_buf);
                handle->image_header_buf = NULL;
            }
            vEventGroupDelete(mqtt_ota_event_group);
            mqtt_ota_event_group = NULL;  /* Set to NULL to prevent dangling pointer */
            break;
        default:
            ESP_LOGE(TAG, "Invalid ESP MQTT OTA State.");
            break;
    }
    if (set_boot_partition && (err == ESP_OK) && (handle->state == ESP_MQTT_OTA_SUCCESS)) {
        err = esp_ota_set_boot_partition(handle->update_partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
        }
    }
    free(handle);
    handle = NULL;
    return err;
}

esp_err_t esp_mqtt_ota_finish(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle)
{
    return esp_mqtt_ota_end(mqtt_ota_handle, true);
}

esp_err_t esp_mqtt_ota_abort(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle)
{
    return esp_mqtt_ota_end(mqtt_ota_handle, false);
}

esp_rmaker_mqtt_ota_state esp_mqtt_ota_get_state(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle)
{
    esp_rmaker_mqtt_ota_t *handle = (esp_rmaker_mqtt_ota_t *)mqtt_ota_handle;
    if (handle == NULL) {
        return ESP_MQTT_OTA_INVALID_STATE;
    }
    return handle->state;
}

int esp_mqtt_ota_get_image_len_read(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle)
{
    esp_rmaker_mqtt_ota_t *handle = (esp_rmaker_mqtt_ota_t *)mqtt_ota_handle;
    if (handle == NULL) {
        return 0;
    }
    return handle->binary_file_len;
}

bool esp_mqtt_ota_is_complete_data_received(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle)
{
    esp_rmaker_mqtt_ota_t *handle = (esp_rmaker_mqtt_ota_t *)mqtt_ota_handle;
    if (handle == NULL) {
        ESP_LOGE(TAG, "esp_rmaker_mqtt_ota_is_complete_data_received: Invalid argument.");
        return false;
    }
    return (handle->binary_file_len == handle->image_length);
}

esp_err_t esp_mqtt_ota_get_img_desc(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle, esp_app_desc_t *new_app_info)
{
    esp_rmaker_mqtt_ota_t *handle = (esp_rmaker_mqtt_ota_t *)mqtt_ota_handle;
    if (handle == NULL || new_app_info == NULL)  {
        ESP_LOGE(TAG, "esp_mqtt_ota_get_img_desc: Invalid argument.");
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->state < ESP_MQTT_OTA_BEGIN) {
        ESP_LOGE(TAG, "esp_mqtt_ota_get_img_desc: Invalid state.");
        return ESP_FAIL;
    }
    if (read_header(handle) != ESP_OK) {
        return ESP_FAIL;
    }

    /* Ensure we have valid header buffer */
    if (!handle->image_header_buf) {
        ESP_LOGE(TAG, "Image header buffer not available");
        return ESP_FAIL;
    }

    /* Validate buffer bounds before accessing app descriptor */
    size_t offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
    if (offset + sizeof(esp_app_desc_t) > IMAGE_HEADER_SIZE) {
        ESP_LOGE(TAG, "App descriptor extends beyond header size");
        return ESP_FAIL;
    }

    /* Use fresh header data from image_header_buf instead of potentially stale ota_upgrade_buf */
    memcpy(new_app_info, &handle->image_header_buf[offset], sizeof(esp_app_desc_t));
    return ESP_OK;
}

/* Constants specific to MQTT OTA */
#ifndef ESP_RMAKER_OTA_RETRY_DELAY_SECONDS
#define ESP_RMAKER_OTA_RETRY_DELAY_SECONDS   (CONFIG_ESP_RMAKER_OTA_RETRY_DELAY_MINUTES * 60)
#endif

/* Local macro for OTA max retries, can be overridden for development */
#ifndef ESP_RMAKER_OTA_MAX_RETRIES
#define ESP_RMAKER_OTA_MAX_RETRIES   CONFIG_ESP_RMAKER_OTA_MAX_RETRIES
#endif

#ifdef CONFIG_ESP_RMAKER_OTA_PROGRESS_SUPPORT
static int last_ota_progress = 0;
static void mqtt_ota_progress_cb(int bytes_read, int total_bytes, void *priv)
{
    esp_rmaker_ota_handle_t ota_handle = (esp_rmaker_ota_handle_t)priv;
    if (ota_handle && total_bytes > 0) {
        int ota_progress = 100 * bytes_read / total_bytes; /* The unit is % */
        /* When ota_progress is 0 or 100, we will not report the progress, because the 0 and 100 is reported by additional_info `Downloading Firmware Image` and
         * `Firmware Image download complete`. And every progress will only report once and the progress is increasing.
         */
        if (((ota_progress != 0) && (ota_progress != 100)) && (ota_progress % CONFIG_ESP_RMAKER_OTA_PROGRESS_INTERVAL == 0) && (last_ota_progress < ota_progress)) {
            last_ota_progress = ota_progress;
            char description[40] = {0};
            snprintf(description, sizeof(description), "Downloaded %d%% Firmware Image", ota_progress);
            esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_IN_PROGRESS, description);
        }
    }
}
esp_err_t esp_mqtt_ota_set_progress_cb(esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle, void (*progress_cb)(int bytes_read, int total_bytes, void *priv), void *priv)
{
    last_ota_progress = 0;
    esp_rmaker_mqtt_ota_t *handle = (esp_rmaker_mqtt_ota_t *)mqtt_ota_handle;
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    handle->progress_cb = progress_cb;
    handle->progress_priv = priv;
    return ESP_OK;
}
#endif

static esp_err_t esp_rmaker_ota_use_mqtt(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data, char *err_desc, size_t err_desc_size) {
    if (!ota_data->stream_id || !ota_data->filesize) {
        snprintf(err_desc, err_desc_size, "Missing stream_id or filesize for MQTT OTA");
        return ESP_FAIL;
    }

    esp_err_t ota_finish_err = ESP_OK;
    esp_rmaker_mqtt_ota_config_t ota_config = {
        .stream_id = ota_data->stream_id,
        .filesize = ota_data->filesize,
        .block_length = CONFIG_ESP_RMAKER_MQTT_OTA_BLOCK_SIZE,
        .blocks_per_request = CONFIG_ESP_RMAKER_MQTT_OTA_NO_OF_BLOCKS,
        .max_retry_count = CONFIG_ESP_RMAKER_MQTT_OTA_MAX_RETRIES
    };
    esp_rmaker_mqtt_ota_handle_t mqtt_ota_handle = NULL;
    if (ota_data->filesize) {
        ESP_LOGD(TAG, "Received file size: %d", ota_data->filesize);
    }

    /* Using a warning just to highlight the message */
    ESP_LOGW(TAG, "Starting OTA. This may take time.");
    esp_err_t err = esp_mqtt_ota_begin(&ota_config, &mqtt_ota_handle);
    if (err != ESP_OK) {
        snprintf(err_desc, err_desc_size, "ESP MQTT OTA Begin failed: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    /* Set up progress reporting callback */
#ifdef CONFIG_ESP_RMAKER_OTA_PROGRESS_SUPPORT
    esp_mqtt_ota_set_progress_cb(mqtt_ota_handle, mqtt_ota_progress_cb, ota_handle);
#endif
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
    err = esp_mqtt_ota_get_img_desc(mqtt_ota_handle, &app_desc);
    if (err != ESP_OK) {
        snprintf(err_desc, err_desc_size, "Failed to read image description: %s", esp_err_to_name(err));
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

    while (1) {
        err = esp_mqtt_ota_perform(mqtt_ota_handle);
        if (err == ESP_ERR_INVALID_VERSION) {
            snprintf(err_desc, err_desc_size, "Chip revision mismatch");
            esp_rmaker_ota_report_status(ota_handle, OTA_STATUS_REJECTED, err_desc);
            /* OTA should be rejected, returning ESP_ERR_INVALID_STATE */
            err = ESP_ERR_INVALID_STATE;
            goto ota_end;
        }
        if (err != ESP_OK && esp_mqtt_ota_get_state(mqtt_ota_handle) != ESP_MQTT_OTA_SUCCESS) {
            if (err != ESP_ERR_TIMEOUT) {  /* Don't break on timeout, just continue */
                break;
            }
        }
        if (esp_mqtt_ota_get_state(mqtt_ota_handle) == ESP_MQTT_OTA_SUCCESS) {
            err = ESP_OK;
            break;
        }

        /* esp_mqtt_ota_perform returns after every read operation which gives user the ability to
         * monitor the status of OTA upgrade by calling esp_mqtt_ota_get_image_len_read, which gives length of image
         * data read so far.
         * We are using a counter just to reduce the number of prints
         */
        count++;
        if (count == 50) {
            ESP_LOGI(TAG, "Image bytes read: %d", esp_mqtt_ota_get_image_len_read(mqtt_ota_handle));
            count = 0;
        }
    }
    if (err != ESP_OK) {
        snprintf(err_desc, err_desc_size, "OTA failed: %s", esp_err_to_name(err));
        /* OTA failed, may retry later */
        goto ota_end;
    }

    if (esp_mqtt_ota_is_complete_data_received(mqtt_ota_handle) != true) {
        snprintf(err_desc, err_desc_size, "Complete data was not received");
        /* OTA failed, may retry later */
        err = ESP_FAIL;
        goto ota_end;
    }

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
        ota_finish_err = esp_mqtt_ota_finish(mqtt_ota_handle);
        if (ota_finish_err == ESP_OK) {
            return ESP_OK;
        } else if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
            snprintf(err_desc, err_desc_size, "Image validation failed");
        } else {
            snprintf(err_desc, err_desc_size, "OTA finish failed: %s", esp_err_to_name(ota_finish_err));
        }
        /* Handle already closed by esp_mqtt_ota_finish(), don't call abort */
        return ESP_FAIL;
    }

    /* Error path: abort the OTA */
    esp_mqtt_ota_abort(mqtt_ota_handle);
    return (err == ESP_ERR_INVALID_STATE) ? ESP_ERR_INVALID_STATE : ESP_FAIL;
}

esp_err_t esp_rmaker_ota_mqtt_cb(esp_rmaker_ota_handle_t ota_handle, esp_rmaker_ota_data_t *ota_data)
{
    if (!ota_data->stream_id || !ota_data->filesize) {
        return ESP_FAIL;
    }

    /* Use the common OTA workflow with MQTT-specific function */
    return esp_rmaker_ota_start_workflow(ota_handle, ota_data, esp_rmaker_ota_use_mqtt, "MQTT");
}
