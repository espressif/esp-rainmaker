/* Zigbee Gateway Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once
#include <esp_err.h>
#include <esp_gateway_zigbee_type.h>

esp_err_t esp_zb_prase_ic_obj(char *payload, esp_zigbee_ic_mac_address_t *ic_mac_value);
