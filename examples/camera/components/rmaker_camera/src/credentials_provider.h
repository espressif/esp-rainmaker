/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

/**
 * @brief Fetch AWS temporary credentials using IoT Core mutual TLS.
 *
 * @param user_data user data
 * @param pAK access key
 * @param pAKLen access key length
 * @param pSK secret key
 * @param pSKLen secret key length
 * @param pTok session token
 * @param pTokLen session token length
 * @param pExp expiration time in seconds from now
 * @return int 0 on success, -1 on failure
 */
int rmaker_fetch_aws_credentials(uint64_t user_data,
                                 const char **pAK, uint32_t *pAKLen,
                                 const char **pSK, uint32_t *pSKLen,
                                 const char **pTok, uint32_t *pTokLen,
                                 uint32_t *pExp);
