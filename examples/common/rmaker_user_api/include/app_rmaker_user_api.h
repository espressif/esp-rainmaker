/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RainMaker User API configuration. Refresh token or username and password is required for authentication.
 */
typedef struct {
    /* Refresh token is used to obtain access tokens for API calls. */
    char *refresh_token;
    /* Base URL is the base URL for the RainMaker User API. */
    char *base_url;
    /* API version for the RainMaker User API. If not set, the API version will be obtained dynamically from the base URL. */
    char *api_version;
    /* Username is the username for the RainMaker account. */
    char *username;
    /* Password is the password for the RainMaker account. */
    char *password;
} app_rmaker_user_api_config_t;

/**
 * @brief RainMaker User API types
 */
typedef enum {
    /* GET API */
    APP_RMAKER_USER_API_TYPE_GET = 0,
    /* POST API */
    APP_RMAKER_USER_API_TYPE_POST,
    /* PUT API */
    APP_RMAKER_USER_API_TYPE_PUT,
    /* DELETE API */
    APP_RMAKER_USER_API_TYPE_DELETE,
} app_rmaker_user_api_type_t;

/**
 * @brief Request configuration for User API
 */
typedef struct {
    /* Whether to reuse the HTTP session, if true, the connection will be reused for subsequent requests. */
    bool reuse_session;
    /* If true, authorization is not required; if false, header will be set with authorization token */
    bool no_need_authorize;
    /* Whether the payload is JSON, if true, header will be set with Content-Type: application/json */
    bool payload_is_json;
    /* API type */
    app_rmaker_user_api_type_t api_type;
    /* API name */
    const char *api_name;
    /* API version, if not set, the default API version will be used */
    const char *api_version;
    /* API payload, if not set, the API payload will be NULL */
    const char *api_payload;
    /* API query params, if not set, the API query params will be NULL */
    const char *api_query_params;
} app_rmaker_user_api_request_config_t;

/**
 * @brief Type definition for login failure callback, will be called when login fails.
 *
 * @param error_code The error code
 * @param failed_reason The reason why login failed
 */
typedef void (*app_rmaker_user_api_login_failure_callback_t)(int error_code, const char *failed_reason);

/**
 * @brief Type definition for login success callback, will be called when login succeeds.
 */
typedef void (*app_rmaker_user_api_login_success_callback_t)(void);

/**
 * @brief Initialize the RainMaker User API
 *
 * @param config The configuration for the Rainmaker User API
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_api_init(app_rmaker_user_api_config_t *config);

/**
 * @brief Deinitialize the RainMaker User API
 *
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_api_deinit(void);

/**
 * @brief Get API version
 *
 * @param api_version Pointer to store API version, caller must free
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_api_get_api_version(char **api_version);

/**
 * @brief Login to RainMaker cloud using refresh token or username and password.
 *
 * @note Do not need to call this function explicitly, other APIs will automatically login when needed.
 *
 * This function attempts to login to the RainMaker cloud using the stored refresh token or username and password.
 * If successful, it will store the access token for subsequent API calls.
 *
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_api_login(void);

/**
 * @brief Register a callback for login failure. Registering the callback is optional; if not registered, login failure will be ignored.
 *
 * This function registers a callback for login failure. The callback will be called when login fails.
 *
 * @param callback The callback function
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_api_register_login_failure_callback(app_rmaker_user_api_login_failure_callback_t callback);

/**
 * @brief Register a callback for login success. Registering the callback is optional; if not registered, login success will be ignored.
 *
 * This function registers a callback for login success. The callback will be called when login succeeds.
 *
 * @param callback The callback function
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_api_register_login_success_callback(app_rmaker_user_api_login_success_callback_t callback);

/**
 * @brief Execute a RainMaker Generic User API.
 *
 * @note This function is used to execute a RainMaker Generic User API.
 * API type, name, version, query params and payload should be set in request config.
 * Can refer to https://swaggerapis.rainmaker.espressif.com/ for API reference details.
 * When the function returns ESP_OK, it only means the API was executed successfully; the status code may not be 200, so the caller should check the status code.
 * When an error code is returned, the response data is not valid and the caller should not use it.
 *
 * @param request_config The request configuration
 * @param status_code Pointer to store status code
 * @param response_data Pointer to store response data, caller must free
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_api_generic(app_rmaker_user_api_request_config_t *request_config, int *status_code, char **response_data);

/**
 * @brief Set refresh token for authentication.
 *
 * This function stores the refresh token that will be used for authentication.
 * The refresh token is used to obtain access tokens for API calls.
 *
 * @param refresh_token The refresh token string
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_api_set_refresh_token(const char *refresh_token);

/**
 * @brief Get current refresh token (copy).
 *
 * This function copies the currently stored refresh token (if any) into a newly
 * allocated string. Caller must free() it.
 *
 * @param[out] refresh_token Pointer to receive allocated refresh token string
 * @return esp_err_t
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if refresh_token is NULL
 *     - ESP_ERR_INVALID_STATE if no refresh token is available
 *     - ESP_ERR_NO_MEM on allocation failure
 */
esp_err_t app_rmaker_user_api_get_refresh_token(char **refresh_token);

/**
 * @brief Set username and password for authentication.
 *
 * This function stores the username and password that will be used for authentication.
 * The username and password are used to obtain access tokens for API calls.
 *
 * @param username The username string
 * @param password The password string
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_api_set_username_password(const char *username, const char *password);

/**
 * @brief Set base URL for RainMaker User API.
 *
 * This function sets the base URL for the RainMaker API.
 *
 * @param base_url The base URL string
 * @return esp_err_t
 *     - ESP_OK on success
 *     - Error code otherwise
 */
esp_err_t app_rmaker_user_api_set_base_url(const char *base_url);

#ifdef __cplusplus
}
#endif
