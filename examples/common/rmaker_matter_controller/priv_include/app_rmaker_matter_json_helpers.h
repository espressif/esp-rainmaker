/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cJSON.h>
#include <esp_err.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lib/core/TLVReader.h>

extern "C" {

/**
 * @brief Convert a Matter TLV value to a cJSON value
 *
 * @param[in] reader The Matter TLV reader positioned at the value
 * @param[out] json The converted cJSON value
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_matter_tlv_to_json(chip::TLV::TLVReader &reader, cJSON **json);

/**
 * @brief Convert a Matter TLV value to a JSON string
 *
 * @param[in] reader The Matter TLV reader positioned at the value
 * @param[out] buf The buffer to store the JSON string
 * @param[in] buf_size The size of buf
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_matter_tlv_to_json_string(chip::TLV::TLVReader *reader, char *buf, size_t buf_size);

}
