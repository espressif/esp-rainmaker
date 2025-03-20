/* Zigbee Gateway Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once
#include <esp_rmaker_core.h>
#include <esp_zigbee_core.h>

#define ZB_IC_KEY_SIZE  16

typedef struct esp_zigbee_ic_mac_address_s{
    uint8_t ic[ZB_IC_KEY_SIZE + 2];                 /* +2 for CRC16 */
    esp_zb_ieee_addr_t addr;                /* 64 bit MAC address */
} esp_zigbee_ic_mac_address_t;

/* define a single remote device struct for managing */
typedef struct device_params_s {
    esp_zb_ieee_addr_t ieee_addr;
    uint8_t  endpoint;
    uint16_t short_addr;
} device_params_t;

typedef struct esp_rainmaker_gateway_end_device_list_s{
   esp_rmaker_device_t *device;
   esp_zb_ha_standard_devices_t device_type;
   device_params_t zigbee_info;
   bool attribute_value;
   struct esp_rainmaker_gateway_end_device_list_s *next;
} esp_rainmaker_gateway_joining_device_list_t;
