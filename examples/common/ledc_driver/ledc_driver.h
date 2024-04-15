/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

/**
 * @brief Initialize the LEDC RGB LED
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t ledc_init(void);

/**
 *
 * @brief Set RGB value for the LED
 *
 * @param[in] red Intensity of Red color (0-100)
 * @param[in] green Intensity of Green color (0-100)
 * @param[in] blue Intensity of Green color (0-100)
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t ledc_set_rgb(uint32_t red, uint32_t green, uint32_t blue);

/**
 * @brief Set HSV value for the LED
 *
 * @param[in] hue Value of hue in arc degrees (0-360)
 * @param[in] saturation Saturation in percentage (0-100)
 * @param[in] value Value (also called Intensity) in percentage (0-100)
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t ledc_set_hsv(uint32_t hue, uint32_t saturation, uint32_t value);

/**
 * @brief Clear (turn off) the LED
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t ledc_clear();
