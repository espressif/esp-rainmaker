/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <esp_secure_boot.h>
#include <esp_efuse.h>

/**
 * @brief Get secure boot digest
 *
 * @return 2D pointer with secure boot digest array
 * @note the memory allocated gets freed with \ref `esp_rmaker_secure_boot_digest_free` API
 */
char** esp_rmaker_get_secure_boot_digest();

/**
 * @brief free secure boot digest buffer
 */
esp_err_t esp_rmaker_secure_boot_digest_free(char **digest);
