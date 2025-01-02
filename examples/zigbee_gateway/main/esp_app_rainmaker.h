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
esp_err_t esp_app_rainmaker_add_joining_device(esp_zb_ha_standard_devices_t device_type, uint16_t short_address, uint8_t endpoint);
esp_err_t esp_app_rainmaker_update_device_param(uint16_t short_address, uint8_t endpoint, bool value);
uint8_t get_zigbee_device_count();
#if !CONFIG_ZIGBEE_INSTALLCODE_ENABLED
void esp_app_enable_zigbee_add_device();
void esp_app_disable_zigbee_add_device();
void esp_rainmaker_report_zigbee_add_device_state(bool new_state);
#endif
