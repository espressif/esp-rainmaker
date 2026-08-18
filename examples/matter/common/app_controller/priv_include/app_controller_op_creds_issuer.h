/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_err.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_controller_client_setup(uint8_t *ipk, size_t ipk_len, uint64_t fabric_id);
void app_controller_register_op_creds_issuer(void);
esp_err_t app_controller_update_noc(uint64_t fabric_id);

#ifdef __cplusplus
}
#endif
