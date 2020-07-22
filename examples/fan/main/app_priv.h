/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define DEFAULT_POWER       true
#define DEFAULT_SPEED       3

extern esp_rmaker_device_t *fan_device;

void app_driver_init(void);
esp_err_t app_fan_set_power(bool power);
esp_err_t app_fan_set_speed(uint8_t speed);
