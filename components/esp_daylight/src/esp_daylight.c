/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <math.h>
#include <time.h>
#include "esp_daylight.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * NOAA Solar Calculator Implementation
 * Reliable sunrise/sunset (UTC) calculations using NOAA equations
 * Reference: https://gml.noaa.gov/grad/solcalc/
 */

/**
 * @brief Howard Hinnant's days_from_civil algorithm (public domain)
 * Convert civil date to days since Unix epoch
 */
static int64_t days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);                 // [0, 399]
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + yoe / 400 + doy; // [0, 146096]
    return era * 146097 + (int)doe - 719468; // days since 1970-01-01
}

/**
 * @brief Get UTC midnight timestamp for given date
 */
static time_t utc_midnight_epoch(int year, int month, int day)
{
    return (time_t)(days_from_civil(year, (unsigned)month, (unsigned)day) * 86400LL);
}

/**
 * @brief Clamp value between min and max
 */
static double clamp(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/**
 * @brief Calculate fractional year gamma in radians
 * Used for NOAA solar equations
 */
static double fractional_year_gamma(int year, int month, int day)
{
    // Day-of-year N (1..365/366)
    static const int mdays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int ly = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
    int N = day;
    for (int i = 1; i < month; i++) {
        N += mdays[i];
    }
    if (ly && month > 2) {
        N += 1;
    }

    // NOAA form with hour≈0 -> gamma ~= 2π/365 * (N-1 - 12/24)
    return 2.0 * M_PI / (ly ? 366.0 : 365.0) * ((double)N - 1.0 - 0.5);
}

/**
 * @brief Calculate Equation of Time in minutes
 * NOAA polynomial approximation
 */
static double equation_of_time_min(double gamma)
{
    double s  = sin(gamma);
    double c  = cos(gamma);
    double s2 = sin(2 * gamma);
    double c2 = cos(2 * gamma);

    // 229.18 * (0.000075 + 0.001868*cosγ - 0.032077*sinγ - 0.014615*cos2γ - 0.040849*sin2γ)
    return 229.18 * (0.000075 + 0.001868 * c - 0.032077 * s - 0.014615 * c2 - 0.040849 * s2);
}

/**
 * @brief Calculate solar declination in radians
 * NOAA polynomial approximation
 */
static double solar_declination_rad(double gamma)
{
    double s  = sin(gamma);
    double c  = cos(gamma);
    double s2 = sin(2 * gamma);
    double c2 = cos(2 * gamma);
    double s3 = sin(3 * gamma);
    double c3 = cos(3 * gamma);

    // δ = 0.006918 - 0.399912 cosγ + 0.070257 sinγ - 0.006758 cos2γ
    //      + 0.000907 sin2γ - 0.002697 cos3γ + 0.00148 sin3γ
    return 0.006918 - 0.399912 * c + 0.070257 * s - 0.006758 * c2
           + 0.000907 * s2 - 0.002697 * c3 + 0.001480 * s3;
}

bool esp_daylight_calc_sunrise_sunset_utc(int year, int month, int day,
                                          double latitude, double longitude,
                                          time_t *sunrise_utc, time_t *sunset_utc)
{
    // Constants
    const double ZENITH_DEG = 90.833; // geometric zenith for sunrise/sunset
    const double Z = ZENITH_DEG * (M_PI / 180.0);
    const double phi = latitude * (M_PI / 180.0);

    // 1) Day parameters
    double gamma = fractional_year_gamma(year, month, day);
    double EoT_min = equation_of_time_min(gamma);
    double decl = solar_declination_rad(gamma);

    // 2) Hour angle at sunrise/sunset (rad)
    // cos(H0) = (cos(Z) - sin(phi) sin(dec)) / (cos(phi) cos(dec))
    double cosH0 = (cos(Z) - sin(phi) * sin(decl)) / (cos(phi) * cos(decl));

    if (cosH0 < -1.0) {
        // Sun above horizon all day (midnight sun) -> no distinct sunrise/sunset
        return false;
    }
    if (cosH0 > 1.0) {
        // Sun below horizon all day (polar night)
        return false;
    }

    double H0 = acos(clamp(cosH0, -1.0, 1.0));     // radians
    double H0_deg = H0 * 180.0 / M_PI;

    // 3) Solar noon in UTC minutes
    // NOAA: SolarNoon_UTC (minutes) = 720 - 4*longitude - EoT
    // (longitude positive east, negative west)
    double solar_noon_min_utc = 720.0 - 4.0 * longitude - EoT_min;

    // 4) Sunrise/Sunset UTC minutes
    // Daylength (minutes) = 8 * H0 (deg)   [since 1 deg hour angle = 4 minutes]
    double delta_min = 4.0 * H0_deg; // minutes from noon to event
    double sunrise_min_utc = solar_noon_min_utc - delta_min;
    double sunset_min_utc  = solar_noon_min_utc + delta_min;

    // Normalize to [0,1440) to stay within the same civil day in UTC.
    // (Edge cases near poles can roll into prev/next day; we keep them bounded.)
    while (sunrise_min_utc < 0) {
        sunrise_min_utc += 1440.0;
    }
    while (sunrise_min_utc >= 1440.0) {
        sunrise_min_utc -= 1440.0;
    }
    while (sunset_min_utc < 0) {
        sunset_min_utc += 1440.0;
    }
    while (sunset_min_utc >= 1440.0) {
        sunset_min_utc -= 1440.0;
    }

    // 5) Convert to epoch seconds (UTC)
    time_t midnight_utc = utc_midnight_epoch(year, month, day);
    if (sunrise_utc) {
        *sunrise_utc = midnight_utc + (time_t)llround(sunrise_min_utc * 60.0);
    }
    if (sunset_utc) {
        *sunset_utc  = midnight_utc + (time_t)llround(sunset_min_utc  * 60.0);
    }

    return true;
}

bool esp_daylight_calc_sunrise_sunset_location(int year, int month, int day,
                                               const esp_daylight_location_t *location,
                                               time_t *sunrise_utc, time_t *sunset_utc)
{
    if (!location) {
        return false;
    }

    return esp_daylight_calc_sunrise_sunset_utc(year, month, day,
                                                location->latitude, location->longitude,
                                                sunrise_utc, sunset_utc);
}

time_t esp_daylight_apply_offset(time_t base_time, int offset_minutes)
{
    return base_time + (offset_minutes * 60);
}

bool esp_daylight_get_sunrise_today(const esp_daylight_location_t *location, time_t *sunrise_utc)
{
    if (!location || !sunrise_utc) {
        return false;
    }

    time_t now;
    time(&now);
    struct tm *tm_info = gmtime(&now);

    return esp_daylight_calc_sunrise_sunset_location(
               tm_info->tm_year + 1900,
               tm_info->tm_mon + 1,
               tm_info->tm_mday,
               location,
               sunrise_utc,
               NULL
           );
}

bool esp_daylight_get_sunset_today(const esp_daylight_location_t *location, time_t *sunset_utc)
{
    if (!location || !sunset_utc) {
        return false;
    }

    time_t now;
    time(&now);
    struct tm *tm_info = gmtime(&now);

    return esp_daylight_calc_sunrise_sunset_location(
               tm_info->tm_year + 1900,
               tm_info->tm_mon + 1,
               tm_info->tm_mday,
               location,
               NULL,
               sunset_utc
           );
}
