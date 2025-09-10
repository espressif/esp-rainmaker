/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/**
 * @brief Location information for daylight calculations
 */
typedef struct {
    double latitude;        /**< Latitude in decimal degrees (-90 to +90, positive North) */
    double longitude;       /**< Longitude in decimal degrees (-180 to +180, positive East) */
    char name[32];         /**< Optional location name for reference */
} esp_daylight_location_t;

/**
 * @brief Calculate sunrise and sunset times for a given date and location
 *
 * Uses NOAA Solar Calculator equations with zenith angle of 90.833° for
 * geometric sunrise/sunset including atmospheric refraction.
 *
 * @param[in] year Year (e.g., 2024)
 * @param[in] month Month (1-12)
 * @param[in] day Day of month (1-31)
 * @param[in] latitude Latitude in decimal degrees (-90 to +90, positive North)
 * @param[in] longitude Longitude in decimal degrees (-180 to +180, positive East)
 * @param[out] sunrise_utc Sunrise time as UTC timestamp (seconds since epoch)
 * @param[out] sunset_utc Sunset time as UTC timestamp (seconds since epoch)
 *
 * @return true on success, false if sun doesn't rise/set (polar day/night)
 */
bool esp_daylight_calc_sunrise_sunset_utc(int year, int month, int day,
                                          double latitude, double longitude,
                                          time_t *sunrise_utc, time_t *sunset_utc);

/**
 * @brief Calculate sunrise and sunset times using location struct
 *
 * @param[in] year Year (e.g., 2024)
 * @param[in] month Month (1-12)
 * @param[in] day Day of month (1-31)
 * @param[in] location Location information
 * @param[out] sunrise_utc Sunrise time as UTC timestamp
 * @param[out] sunset_utc Sunset time as UTC timestamp
 *
 * @return true on success, false if sun doesn't rise/set (polar day/night)
 */
bool esp_daylight_calc_sunrise_sunset_location(int year, int month, int day,
                                               const esp_daylight_location_t *location,
                                               time_t *sunrise_utc, time_t *sunset_utc);

/**
 * @brief Apply time offset to a base timestamp
 *
 * @param[in] base_time Base timestamp in seconds since epoch
 * @param[in] offset_minutes Offset in minutes (positive = after, negative = before)
 *
 * @return Adjusted timestamp
 */
time_t esp_daylight_apply_offset(time_t base_time, int offset_minutes);

/**
 * @brief Get sunrise time for current date at given location
 *
 * @param[in] location Location information
 * @param[out] sunrise_utc Sunrise time as UTC timestamp
 *
 * @return true on success, false on failure
 */
bool esp_daylight_get_sunrise_today(const esp_daylight_location_t *location, time_t *sunrise_utc);

/**
 * @brief Get sunset time for current date at given location
 *
 * @param[in] location Location information
 * @param[out] sunset_utc Sunset time as UTC timestamp
 *
 * @return true on success, false on failure
 */
bool esp_daylight_get_sunset_today(const esp_daylight_location_t *location, time_t *sunset_utc);

#ifdef __cplusplus
}
#endif
