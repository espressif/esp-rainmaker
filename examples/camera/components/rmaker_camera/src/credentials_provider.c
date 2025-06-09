/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "credentials_provider.h"
#include "esp_log.h"
#include <esp_rmaker_core.h>
#include <esp_rmaker_factory.h>

static const char *TAG = "credentials";

/* Global credentials pointer - allows direct access without copying */
static esp_rmaker_aws_credentials_t *g_current_credentials = NULL;

/* Assume the videostream role and get the aws credentials */
int rmaker_fetch_aws_credentials(uint64_t user_data,
                                 const char **pAK, uint32_t *pAKLen,
                                 const char **pSK, uint32_t *pSKLen,
                                 const char **pTok, uint32_t *pTokLen,
                                 uint32_t *pExp)
{
    (void)user_data;

    /* Free previous credentials if any */
    if (g_current_credentials) {
        esp_rmaker_free_aws_credentials(g_current_credentials);
        g_current_credentials = NULL;
    }

    /* Get fresh credentials */
    g_current_credentials = esp_rmaker_get_aws_security_token("esp-videostream-v1-NodeRole");
    if (!g_current_credentials) {
        ESP_LOGE(TAG, "Failed to get AWS security token");
        return -1;
    }

    /* Return pointers directly to credential fields */
    *pAK = g_current_credentials->access_key;
    *pAKLen = (uint32_t)strlen(g_current_credentials->access_key);
    *pSK = g_current_credentials->secret_key;
    *pSKLen = (uint32_t)strlen(g_current_credentials->secret_key);
    *pTok = g_current_credentials->session_token;
    *pTokLen = (uint32_t)strlen(g_current_credentials->session_token);
    *pExp = g_current_credentials->expiration;

    ESP_LOGI(TAG, "Successfully fetched AWS credentials using RainMaker core function");
    return 0;
}
