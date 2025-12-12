/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) PTE LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_log.h>
#include <esp_rmaker_common_console.h>
#include <esp_rmaker_console.h>

esp_err_t esp_rmaker_console_init()
{
    esp_rmaker_common_console_init();
    esp_rmaker_register_commands();
    return ESP_OK;
}
