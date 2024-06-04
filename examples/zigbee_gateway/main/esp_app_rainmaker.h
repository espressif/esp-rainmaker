/* Zigbee Gateway Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once
#include <esp_err.h>
#include <esp_zigbee_core.h>

void esp_app_rainmaker_main(void);
esp_err_t esp_app_rainmaker_joining_add_device(esp_zb_ha_standard_devices_t device_type, uint16_t short_address, uint8_t endpoint);
