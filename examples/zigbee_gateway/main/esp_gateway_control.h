/* Zigbee Gateway Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once
#include <esp_err.h>
#include <esp_zigbee_core.h>
#include <esp_gateway_param_parser.h>

#define ESP_ZIGBEE_GATWAY_OPEN_NETWORK_DEFAULT_TIME 180 /* zigbee open network timeout 180 seconds*/

esp_err_t esp_gateway_control_permit_join(uint8_t permit_duration);
esp_err_t esp_gateway_control_secur_ic_add(esp_zigbee_ic_mac_address_t *IC_MacAddress, esp_zb_secur_ic_type_t ic_type);
esp_err_t esp_gateway_control_light_on_off(bool on_off, device_params_t *device);
