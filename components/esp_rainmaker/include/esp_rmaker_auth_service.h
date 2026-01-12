/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include <esp_event.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** ESP RainMaker Auth Service Event Base */
ESP_EVENT_DECLARE_BASE(RMAKER_AUTH_SERVICE_EVENT);

/** ESP RainMaker Auth Service Events */
typedef enum {
    /** Authentication service enabled */
    RMAKER_AUTH_SERVICE_EVENT_ENABLED,
    /** Authentication service token received */
    RMAKER_AUTH_SERVICE_EVENT_TOKEN_RECEIVED,
    /** Authentication service base URL received */
    RMAKER_AUTH_SERVICE_EVENT_BASE_URL_RECEIVED,
    /** Authentication service disabled */
    RMAKER_AUTH_SERVICE_EVENT_DISABLED,
} esp_rmaker_auth_service_event_t;

/** User Auth Service Token Status */
typedef enum {
    ESP_RMAKER_USER_AUTH_SERVICE_TOKEN_STATUS_NONE = 0,
    ESP_RMAKER_USER_AUTH_SERVICE_TOKEN_STATUS_NOT_VERIFIED = 1,
    ESP_RMAKER_USER_AUTH_SERVICE_TOKEN_STATUS_VERIFIED = 2,
    ESP_RMAKER_USER_AUTH_SERVICE_TOKEN_STATUS_EXPIRED_OR_INVALID = 3,
    ESP_RMAKER_USER_AUTH_SERVICE_TOKEN_STATUS_MAX,
} esp_rmaker_user_auth_service_token_status_t;

/** Enable User Auth Service
 *
 * This API enables the user authentication service for the node.
 * @note This API currently needs to be called before esp_rmaker_start() to ensure the node details are reported to the cloud.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_auth_service_enable(void);

/** Disable User Auth Service
 *
 * This API disables the user authentication service for the node.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_auth_service_disable(void);

/**
 * Update the user token status
 *
 * This function updates the user token status. The status is used to determine if the user token is valid or not.
 *
 * @param[in] status The user token status to update.
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t esp_rmaker_user_auth_service_token_status_update(esp_rmaker_user_auth_service_token_status_t status);

/** Get the user token
 *
 * This function gets the user token.
 *
 * @param[out] user_token The user token. Should be freed by the caller.
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t esp_rmaker_auth_service_get_user_token(char **user_token);

/** Get the base URL
 *
 * This function gets the base URL.
 *
 * @param[out] base_url The base URL. Should be freed by the caller.
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t esp_rmaker_auth_service_get_base_url(char **base_url);
#ifdef __cplusplus
}
#endif
