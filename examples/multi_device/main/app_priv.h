/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define DEFAULT_SWITCH_POWER        true
#define DEFAULT_LIGHT_POWER         true
#define DEFAULT_LIGHT_BRIGHTNESS    25
#define DEFAULT_FAN_POWER           false
#define DEFAULT_FAN_SPEED           3
#define DEFAULT_TEMPERATURE         25.0
#define REPORTING_PERIOD            60 /* Seconds */

extern esp_rmaker_device_t *switch_device;
extern esp_rmaker_device_t *light__device;
extern esp_rmaker_device_t *fan_device;
extern esp_rmaker_device_t *temp_sensor_device;

void app_driver_init(void);
int app_driver_set_state(bool state);
bool app_driver_get_state(void);
float app_get_current_temperature();
