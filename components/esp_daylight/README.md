# ESP Daylight Component

A standalone C library for calculating sunrise and sunset times using NOAA Solar Calculator equations. This component provides accurate solar calculations for any location and date, designed for integration with ESP-IDF projects.

## Features

- **Accurate NOAA calculations**: Uses proven NOAA Solar Calculator equations
- **Global coverage**: Works for any location worldwide (handles polar regions)
- **Lightweight**: Minimal memory footprint and dependencies
- **ESP-IDF ready**: Designed for embedded systems
- **Time zone agnostic**: Returns UTC timestamps for easy conversion

## API Reference

### Core Functions

```c
bool esp_daylight_calc_sunrise_sunset_utc(int year, int month, int day,
                                          double latitude, double longitude,
                                          time_t *sunrise_utc, time_t *sunset_utc);
```

Calculate sunrise and sunset times for a specific date and location.

**Parameters:**
- `year`: Year (e.g., 2024)
- `month`: Month (1-12)
- `day`: Day of month (1-31)
- `latitude`: Latitude in decimal degrees (-90 to +90, positive North)
- `longitude`: Longitude in decimal degrees (-180 to +180, positive East)
- `sunrise_utc`: Output sunrise time as UTC timestamp
- `sunset_utc`: Output sunset time as UTC timestamp

**Returns:** `true` on success, `false` if sun doesn't rise/set (polar regions)

### Helper Functions

```c
time_t esp_daylight_apply_offset(time_t base_time, int offset_minutes);
```

Apply time offset to a base timestamp.

```c
bool esp_daylight_get_sunrise_today(const esp_daylight_location_t *location, time_t *sunrise_utc);
bool esp_daylight_get_sunset_today(const esp_daylight_location_t *location, time_t *sunset_utc);
```

Get sunrise/sunset for current date at given location.

## Usage Example

```c
#include "esp_daylight.h"

void calculate_solar_times(void) {
    time_t sunrise_utc, sunset_utc;

    // Calculate for Pune, India on August 29, 2025
    bool ok = esp_daylight_calc_sunrise_sunset_utc(
        2025, 8, 29,           // Date
        18.5204, 73.8567,      // Pune coordinates
        &sunrise_utc, &sunset_utc
    );

    if (ok) {
        // Apply offset: 30 minutes before sunset
        time_t light_on_time = esp_daylight_apply_offset(sunset_utc, -30);

        struct tm *tm_info = gmtime(&light_on_time);
        printf("Turn on light at: %02d:%02d:%02d UTC\n",
               tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    } else {
        printf("No sunrise/sunset for this location and date\n");
    }
}
```

## Location Coordinates

### Popular Cities

| City | Latitude | Longitude |
|------|----------|-----------|
| New York | 40.7128 | -74.0060 |
| London | 51.5074 | -0.1278 |
| Pune | 18.5204 | 73.8567 |
| Shanghai | 31.2304 | 121.4737 |
| Sydney | -33.8688 | 151.2093 |

### Coordinate Format

- **Latitude**: -90 to +90 degrees (positive = North, negative = South)
- **Longitude**: -180 to +180 degrees (positive = East, negative = West)

## Integration with ESP Schedule

This component integrates seamlessly with ESP Schedule for solar-based scheduling:

```c
esp_schedule_config_t schedule_config = {
    .name = "sunset_light",
    .trigger.type = ESP_SCHEDULE_TYPE_SUNSET,
    .trigger.solar.latitude = 18.5204,
    .trigger.solar.longitude = 73.8567,
    .trigger.solar.offset_minutes = -30,  // 30 min before sunset
    .trigger.solar.repeat_days = ESP_SCHEDULE_DAY_EVERYDAY,
    .trigger_cb = light_control_callback,
};
```

## Algorithm Details

The component implements the NOAA Solar Calculator algorithm with:

- **Zenith angle**: 90.833° (accounts for atmospheric refraction)
- **Precision**: Typically accurate to within 1-2 minutes
- **Edge cases**: Handles polar day/night conditions gracefully

## Dependencies

- ESP-IDF (esp_common)
- Standard C math library (`-lm`)

## Component Structure

```
esp_daylight/
├── CMakeLists.txt
├── include/
│   └── esp_daylight.h
├── src/
│   └── esp_daylight.c
└── README.md
```
