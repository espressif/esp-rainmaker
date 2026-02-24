/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* RainMaker User API core (init, login, execute request, token/URL config) */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <esp_log.h>
#include <cJSON.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include "app_rmaker_user_api.h"
#include "app_rmaker_user_api_util.h"

#ifdef CONFIG_RM_USER_SUPPORT_REUSE_HTTP_SESSION
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
#define RM_USER_API_REUSE_HTTP_SESSION
#else
#warning "Reuse HTTP session, needs IDF version >= 5.4.0, please update your IDF version"
#endif
#endif

/**
 * @brief RainMaker User API context structure
 */
typedef struct {
    bool            is_initialized;      /**< Whether the User API is initialized */
    bool            api_version_checked; /**< Whether the API version has been checked */
    SemaphoreHandle_t api_lock;          /**< Mutex for API serialization */
    char           *api_version;         /**< API version string */
    char           *access_token;        /**< Current access token */
    char           *refresh_token;       /**< Stored refresh token */
    char           *base_url;            /**< Base URL for RainMaker User API */
    char           *username;           /**< Username for login */
    char           *password;            /**< Password for login */
    esp_http_client_handle_t http_client; /**< HTTP client handle (persistent connection) */
    app_rmaker_user_api_login_failure_callback_t login_failure_callback; /**< Login failure callback */
    app_rmaker_user_api_login_success_callback_t login_success_callback;  /**< Login success callback */
} app_rmaker_user_api_context_t;

/* Global context */
static const char *TAG = "rmaker_user_api";
static const char *rmaker_user_api_versions_url = "apiversions";
static const char *rmaker_user_api_login_url = "login2";

static app_rmaker_user_api_context_t g_rmaker_user_api_ctx = {0};

#define APP_RMAKER_USER_API_URL_LEN                 256
#define APP_RMAKER_USER_API_HTTP_TX_BUFFER_SIZE     2048
#define APP_RMAKER_USER_API_DEFAULT_BASE_URL        "https://api.rainmaker.espressif.com"
#define APP_RMAKER_USER_API_LOCK()                  xSemaphoreTakeRecursive(g_rmaker_user_api_ctx.api_lock, portMAX_DELAY)
#define APP_RMAKER_USER_API_UNLOCK()                xSemaphoreGiveRecursive(g_rmaker_user_api_ctx.api_lock)

/**
 * @brief Safe string copy helper
 */
char *app_rmaker_user_api_safe_strdup(const char *str)
{
    if (!str) {
        return NULL;
    }
    return strdup(str);
}

/**
 * @brief Safe string free helper
 */
void app_rmaker_user_api_safe_free(char **ptr)
{
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

/**
 * @brief Get HTTP method
 */
static esp_http_client_method_t app_rmaker_user_api_get_http_method(app_rmaker_user_api_type_t api_type)
{
    switch (api_type) {
        case APP_RMAKER_USER_API_TYPE_GET:
            return HTTP_METHOD_GET;
        case APP_RMAKER_USER_API_TYPE_POST:
            return HTTP_METHOD_POST;
        case APP_RMAKER_USER_API_TYPE_PUT:
            return HTTP_METHOD_PUT;
        case APP_RMAKER_USER_API_TYPE_DELETE:
            return HTTP_METHOD_DELETE;
        default:
            return HTTP_METHOD_GET;
    }
    return HTTP_METHOD_GET;
}

/**
 * @brief Check whether base URL and (username/password or refresh token) are set
 */
static bool app_rmaker_user_api_credentials_check(void)
{
    if (!g_rmaker_user_api_ctx.base_url) {
        ESP_LOGE(TAG, "Base URL is not set, please use app_rmaker_user_api_set_base_url to set it");
        return false;
    }
    if (!g_rmaker_user_api_ctx.username || !g_rmaker_user_api_ctx.password) {
        if (!g_rmaker_user_api_ctx.refresh_token) {
            ESP_LOGE(TAG,
                     "Username and password or refresh token is not set, please use "
                     "app_rmaker_user_api_set_username_password or app_rmaker_user_api_set_refresh_token to set it");
            return false;
        }
    }
    return true;
}

/**
 * @brief Clear user info when the user account or base URL is changed
 */
static void app_rmaker_user_api_clear_user_info(void)
{
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.access_token);
}

/**
 * @brief Set common HTTP headers
 */
static void app_rmaker_user_api_set_http_headers(esp_http_client_handle_t client, bool payload_is_json, bool need_authorize)
{
    if (need_authorize) {
        if (g_rmaker_user_api_ctx.access_token) {
            esp_http_client_set_header(client, "Authorization", g_rmaker_user_api_ctx.access_token);
        }
    }
    if (payload_is_json) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
    } else {
        esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    }
    esp_http_client_set_header(client, "accept", "application/json");
}

/**
 * @brief Make HTTP request
 */
static esp_err_t app_rmaker_user_api_make_http_request(esp_http_client_handle_t client, const char *post_data, bool reuse_connection)
{
    if (!client) {
        ESP_LOGE(TAG, "Client is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    if (reuse_connection) {
#ifdef RM_USER_API_REUSE_HTTP_SESSION
        err = esp_http_client_prepare(client);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to prepare HTTP client: %s", esp_err_to_name(err));
            return err;
        }
        err = esp_http_client_request_send(client, post_data ? strlen(post_data) : 0);
#else
        ESP_LOGE(TAG, "Reuse HTTP session is not supported, please update your ESP-IDF version to >= 5.4.0");
        return ESP_ERR_NOT_SUPPORTED;
#endif
    } else {
        err = esp_http_client_open(client, post_data ? strlen(post_data) : 0);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        return err;
    }

    if (post_data) {
        ESP_LOGD(TAG, "Sending data: %s", post_data);
        int wlen = esp_http_client_write(client, post_data, strlen(post_data));
        if (wlen != strlen(post_data)) {
            ESP_LOGE(TAG, "Write failed, returned len: %d, need write len: %d", wlen, strlen(post_data));
            return ESP_ERR_HTTP_WRITE_DATA;
        }
    }

    ESP_LOGD(TAG, "HTTP request sent successfully");
    return ESP_OK;
}

/**
 * @brief Read HTTP response
 */
static esp_err_t app_rmaker_user_api_read_http_response(esp_http_client_handle_t client, char **response_data, int *status_code)
{
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGE(TAG, "Failed to fetch headers");
        return ESP_ERR_HTTP_FETCH_HEADER;
    }

    *status_code = esp_http_client_get_status_code(client);
    *response_data = MEM_CALLOC_EXTRAM(1, content_length + 1);
    if (!*response_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        return ESP_ERR_NO_MEM;
    }

    int data_read_size = content_length;
    int bytes_read = 0, data_read = 0;
    while (data_read_size > 0) {
        data_read = esp_http_client_read(client, (*response_data + bytes_read), data_read_size);
        if (data_read < 0) {
            if (data_read == -ESP_ERR_HTTP_EAGAIN) {
                ESP_LOGD(TAG, "ESP_ERR_HTTP_EAGAIN invoked: Call timed out before data was ready");
                continue;
            }
            ESP_LOGE(TAG, "Connection closed, errno = %d", errno);
            break;
        }
        data_read_size -= data_read;
        bytes_read += data_read;
    }
    if (data_read_size > 0) {
        ESP_LOGE(TAG, "Complete data was not received");
        app_rmaker_user_api_safe_free(response_data);
        *status_code = 0;
        return ESP_FAIL;
    }
    (*response_data)[bytes_read] = '\0';
    return ESP_OK;
}

/**
 * @brief Handle HTTP response and check for authentication errors
 */
static esp_err_t app_rmaker_user_api_handle_http_response(esp_http_client_handle_t client, int *status_code, char **response_data, bool *need_retry)
{
    esp_err_t err = app_rmaker_user_api_read_http_response(client, response_data, status_code);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGD(TAG, "Status code: %d, response_data: %s", *status_code, *response_data);

    /* Handle unauthorized error */
    if (*status_code == 401 && strstr(*response_data, "Unauthorized")) {
        ESP_LOGW(TAG, "Access token expired, attempting re-login");
        /* Try to login again */
        if (app_rmaker_user_api_login() == ESP_OK) {
            *need_retry = true;
        } else {
            *need_retry = false;
        }
    }

    return ESP_OK;
}

/**
 * @brief Create JSON payload with refresh token
 */
static char *app_rmaker_user_api_create_login_payload(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return NULL;
    }

    /* If username and password are set, use them to login */
    if (g_rmaker_user_api_ctx.username && g_rmaker_user_api_ctx.password) {
        cJSON_AddStringToObject(root, "user_name", g_rmaker_user_api_ctx.username);
        cJSON_AddStringToObject(root, "password", g_rmaker_user_api_ctx.password);
    } else if (g_rmaker_user_api_ctx.refresh_token) {
        cJSON_AddStringToObject(root, "refreshtoken", g_rmaker_user_api_ctx.refresh_token);
    } else {
        ESP_LOGE(TAG,
                 "Username and password or refresh token is not set, please use app_rmaker_user_api_set_username_password "
                 "or app_rmaker_user_api_set_refresh_token to set it");
        return NULL;
    }
    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

/**
 * @brief Parse login response
 */
static esp_err_t app_rmaker_user_api_parse_login_response(const char *response_data)
{
    cJSON *response = cJSON_Parse(response_data);
    if (!response) {
        ESP_LOGE(TAG, "Failed to parse response JSON");
        return ESP_FAIL;
    }

    cJSON *status = cJSON_GetObjectItem(response, "status");
    if (!status || !status->valuestring || strcmp(status->valuestring, "success") != 0) {
        cJSON *description = cJSON_GetObjectItem(response, "description");
        ESP_LOGE(TAG, "Login failed: %s",
                 description && description->valuestring ? description->valuestring : "Unknown error");
        cJSON_Delete(response);
        return ESP_FAIL;
    }

    cJSON *access_token_json = cJSON_GetObjectItem(response, "accesstoken");
    if (!access_token_json || !access_token_json->valuestring) {
        ESP_LOGE(TAG, "No access token in response");
        cJSON_Delete(response);
        return ESP_FAIL;
    }

    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.access_token);
    g_rmaker_user_api_ctx.access_token = app_rmaker_user_api_safe_strdup(access_token_json->valuestring);
    ESP_LOGD(TAG, "Access token saved successfully");

    /* Save refresh token if provided (login2 response includes refreshtoken on username/password login). */
    cJSON *refresh_token_json = cJSON_GetObjectItem(response, "refreshtoken");
    if (refresh_token_json && cJSON_IsString(refresh_token_json) && refresh_token_json->valuestring) {
        app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.refresh_token);
        g_rmaker_user_api_ctx.refresh_token = app_rmaker_user_api_safe_strdup(refresh_token_json->valuestring);
        if (g_rmaker_user_api_ctx.refresh_token) {
            ESP_LOGD(TAG, "Refresh token saved successfully");
        }
    }

    if (g_rmaker_user_api_ctx.login_success_callback) {
        g_rmaker_user_api_ctx.login_success_callback();
    }
    cJSON_Delete(response);
    return ESP_OK;
}

/**
 * @brief Cleanup RainMaker User API context
 */
static void app_rmaker_user_api_cleanup_context(void)
{
    app_rmaker_user_api_clear_user_info();
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.username);
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.password);
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.refresh_token);
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.base_url);
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.api_version);
    if (g_rmaker_user_api_ctx.http_client) {
        esp_http_client_cleanup(g_rmaker_user_api_ctx.http_client);
        g_rmaker_user_api_ctx.http_client = NULL;
    }
    if (g_rmaker_user_api_ctx.api_lock) {
        vSemaphoreDelete(g_rmaker_user_api_ctx.api_lock);
        g_rmaker_user_api_ctx.api_lock = NULL;
    }
    g_rmaker_user_api_ctx.api_version_checked = false;
    g_rmaker_user_api_ctx.login_failure_callback = NULL;
    g_rmaker_user_api_ctx.login_success_callback = NULL;
}

/* ---------- Public API implementation ---------- */

esp_err_t app_rmaker_user_api_init(app_rmaker_user_api_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Configuration is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (g_rmaker_user_api_ctx.is_initialized) {
        ESP_LOGE(TAG, "RainMaker User API is already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    g_rmaker_user_api_ctx.refresh_token = app_rmaker_user_api_safe_strdup(config->refresh_token);
    g_rmaker_user_api_ctx.base_url = app_rmaker_user_api_safe_strdup(config->base_url ? config->base_url : APP_RMAKER_USER_API_DEFAULT_BASE_URL);
    g_rmaker_user_api_ctx.username = app_rmaker_user_api_safe_strdup(config->username);
    g_rmaker_user_api_ctx.password = app_rmaker_user_api_safe_strdup(config->password);
    g_rmaker_user_api_ctx.api_version = app_rmaker_user_api_safe_strdup(config->api_version);
    /* If the API version is set, set the API version checked to true, otherwise set it to false */
    g_rmaker_user_api_ctx.api_version_checked = g_rmaker_user_api_ctx.api_version ? true : false;
    g_rmaker_user_api_ctx.api_lock = xSemaphoreCreateRecursiveMutex();
    if (!g_rmaker_user_api_ctx.api_lock) {
        ESP_LOGE(TAG, "Failed to create API lock");
        app_rmaker_user_api_cleanup_context();
        return ESP_ERR_NO_MEM;
    }
    g_rmaker_user_api_ctx.is_initialized = true;
    return ESP_OK;
}

esp_err_t app_rmaker_user_api_deinit(void)
{
    if (!g_rmaker_user_api_ctx.is_initialized) {
        ESP_LOGE(TAG, "RainMaker User API is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    app_rmaker_user_api_cleanup_context();
    g_rmaker_user_api_ctx.is_initialized = false;
    return ESP_OK;
}

esp_err_t app_rmaker_user_api_register_login_failure_callback(app_rmaker_user_api_login_failure_callback_t callback)
{
    if (!callback) {
        ESP_LOGE(TAG, "Callback is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (g_rmaker_user_api_ctx.login_failure_callback) {
        ESP_LOGW(TAG, "Login failure callback is already registered, overriding it");
    }
    g_rmaker_user_api_ctx.login_failure_callback = callback;
    return ESP_OK;
}

esp_err_t app_rmaker_user_api_register_login_success_callback(app_rmaker_user_api_login_success_callback_t callback)
{
    if (!callback) {
        ESP_LOGE(TAG, "Callback is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (g_rmaker_user_api_ctx.login_success_callback) {
        ESP_LOGW(TAG, "Login success callback is already registered, overriding it");
    }
    g_rmaker_user_api_ctx.login_success_callback = callback;
    return ESP_OK;
}

/* GET /apiversions */
esp_err_t app_rmaker_user_api_get_api_version(char **api_version)
{
    if (!api_version) {
        ESP_LOGE(TAG, "API version is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    char *response_data = NULL;
    *api_version = NULL;
    int status_code = 0;

    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .no_need_authorize = true,
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = rmaker_user_api_versions_url,
        .api_version = NULL,
        .api_query_params = NULL,
        .api_payload = NULL,
    };

    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to execute get API version request");
        return err;
    }

    cJSON *response = cJSON_Parse(response_data);
    app_rmaker_user_api_safe_free(&response_data);
    if (response) {
        cJSON *supported_versions = cJSON_GetObjectItem(response, "supported_versions");
        if (supported_versions && cJSON_IsArray(supported_versions)) {
            cJSON *version = cJSON_GetArrayItem(supported_versions, 0);
            if (version && cJSON_IsString(version)) {
                *api_version = app_rmaker_user_api_safe_strdup(version->valuestring);
            }
        } else {
            ESP_LOGE(TAG, "No supported versions found in response");
            err = ESP_FAIL;
        }
        cJSON_Delete(response);
    } else {
        ESP_LOGE(TAG, "Failed to parse API version response");
        err = ESP_FAIL;
    }
    return err;
}

/* POST /{version}/login2 */
esp_err_t app_rmaker_user_api_login(void)
{
    if (!app_rmaker_user_api_credentials_check()) {
        return ESP_ERR_INVALID_STATE;
    }

    const char *payload = app_rmaker_user_api_create_login_payload();
    if (!payload) {
        ESP_LOGE(TAG, "Failed to create login payload");
        return ESP_FAIL;
    }
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .no_need_authorize = true,
        .api_type = APP_RMAKER_USER_API_TYPE_POST,
        .api_name = rmaker_user_api_login_url,
        .api_version = g_rmaker_user_api_ctx.api_version,
        .api_query_params = NULL,
        .api_payload = payload,
    };

    int status_code = 0;
    char *response_data = NULL;
    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    free((char *)payload);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to execute login request");
        return err;
    }

    /* Get response status code */
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP POST request failed with status code: %d", status_code);
        if (response_data) {
            cJSON *response = cJSON_Parse(response_data);
            app_rmaker_user_api_safe_free(&response_data);
            if (response) {
                cJSON *error = cJSON_GetObjectItem(response, "error_code");
                cJSON *description = cJSON_GetObjectItem(response, "description");
                if (error && cJSON_IsNumber(error) && description && cJSON_IsString(description)) {
                    if (g_rmaker_user_api_ctx.login_failure_callback) {
                        g_rmaker_user_api_ctx.login_failure_callback(error->valueint, description->valuestring);
                    }
                }
                cJSON_Delete(response);
            }
        }
        return ESP_FAIL;
    }

    /* Parse response and update access token */
    err = app_rmaker_user_api_parse_login_response(response_data);
    app_rmaker_user_api_safe_free(&response_data);
    return err;
}

esp_err_t app_rmaker_user_api_set_refresh_token(const char *refresh_token)
{
    if (!refresh_token) {
        ESP_LOGE(TAG, "Refresh token is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.refresh_token);
    /* Set refresh token */
    g_rmaker_user_api_ctx.refresh_token = app_rmaker_user_api_safe_strdup(refresh_token);
    if (!g_rmaker_user_api_ctx.refresh_token) {
        ESP_LOGE(TAG, "Failed to allocate memory for refresh token");
        return ESP_ERR_NO_MEM;
    }

    /* Clear user info */
    app_rmaker_user_api_clear_user_info();
    return ESP_OK;
}

esp_err_t app_rmaker_user_api_get_refresh_token(char **refresh_token)
{
    if (!refresh_token) {
        ESP_LOGE(TAG, "Refresh token output is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *refresh_token = NULL;
    if (!g_rmaker_user_api_ctx.refresh_token) {
        return ESP_ERR_INVALID_STATE;
    }
    *refresh_token = app_rmaker_user_api_safe_strdup(g_rmaker_user_api_ctx.refresh_token);
    if (!*refresh_token) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t app_rmaker_user_api_set_username_password(const char *username, const char *password)
{
    if (!username || !password) {
        ESP_LOGE(TAG, "Username or password is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.username);
    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.password);
    g_rmaker_user_api_ctx.username = app_rmaker_user_api_safe_strdup(username);
    g_rmaker_user_api_ctx.password = app_rmaker_user_api_safe_strdup(password);
    if (!g_rmaker_user_api_ctx.username || !g_rmaker_user_api_ctx.password) {
        ESP_LOGE(TAG, "Failed to allocate memory for username or password");
        app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.username);
        app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.password);
        return ESP_ERR_NO_MEM;
    }

    /* Clear user info */
    app_rmaker_user_api_clear_user_info();
    return ESP_OK;
}

esp_err_t app_rmaker_user_api_set_base_url(const char *base_url)
{
    if (!base_url) {
        ESP_LOGE(TAG, "Base URL is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    app_rmaker_user_api_safe_free(&g_rmaker_user_api_ctx.base_url);
    g_rmaker_user_api_ctx.base_url = app_rmaker_user_api_safe_strdup(base_url);
    if (!g_rmaker_user_api_ctx.base_url) {
        ESP_LOGE(TAG, "Failed to allocate memory for base URL");
        return ESP_ERR_NO_MEM;
    }

    /* Clear user info */
    app_rmaker_user_api_clear_user_info();
    return ESP_OK;
}

esp_err_t app_rmaker_user_api_generic(app_rmaker_user_api_request_config_t *request_config, int *status_code, char **response_data)
{
    if (!request_config || !request_config->api_name || !response_data) {
        ESP_LOGE(TAG, "Request config or response data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    *response_data = NULL;
    *status_code = 0;
    /* Check the api type, query params and api payload */
    if (request_config->api_type == APP_RMAKER_USER_API_TYPE_GET) {
        if (!request_config->api_query_params) {
            ESP_LOGW(TAG, "Query params are empty for GET %s API, please check the API documentation", request_config->api_name);
        }
    } else if (request_config->api_type == APP_RMAKER_USER_API_TYPE_POST) {
        if (!request_config->api_payload) {
            ESP_LOGE(TAG, "API payload is required for POST %s API", request_config->api_name);
            return ESP_ERR_INVALID_ARG;
        }
    } else if (request_config->api_type == APP_RMAKER_USER_API_TYPE_PUT) {
        if (!request_config->api_payload) {
            ESP_LOGE(TAG, "API payload is required for PUT %s API", request_config->api_name);
            return ESP_ERR_INVALID_ARG;
        }
    } else if (request_config->api_type == APP_RMAKER_USER_API_TYPE_DELETE) {
        if (!request_config->api_query_params) {
            ESP_LOGW(TAG, "Query params are required for DELETE %s API", request_config->api_name);
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        ESP_LOGE(TAG, "Invalid API type for %s API", request_config->api_name);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_FAIL;
    /* Lock the API to make sure the API is not executed concurrently */
    APP_RMAKER_USER_API_LOCK();
    /* Check whether API version is set; if not, get it first */
    if (!request_config->api_version && !g_rmaker_user_api_ctx.api_version && !g_rmaker_user_api_ctx.api_version_checked) {
        g_rmaker_user_api_ctx.api_version_checked = true;
        err = app_rmaker_user_api_get_api_version(&g_rmaker_user_api_ctx.api_version);
        if (err != ESP_OK) {
            g_rmaker_user_api_ctx.api_version_checked = false;
            goto unlock;
        }
    }

    /* Check access token and whether need authorization */
    if (!request_config->no_need_authorize) {
        if (!g_rmaker_user_api_ctx.access_token) {
            ESP_LOGW(TAG, "Access token not available, need login first");
            err = app_rmaker_user_api_login();
            if (err != ESP_OK) {
                goto unlock;
            }
            return app_rmaker_user_api_generic(request_config, status_code, response_data);
        }
    }

    /* Create URL */
    char url[APP_RMAKER_USER_API_URL_LEN] = {0};
    if (strncmp(request_config->api_name, rmaker_user_api_versions_url, strlen(rmaker_user_api_versions_url)) == 0) {
        snprintf(url, sizeof(url), "%s/%s", g_rmaker_user_api_ctx.base_url, request_config->api_name);
    } else {
        if (request_config->api_query_params) {
            snprintf(url, sizeof(url), "%s/%s/%s?%s", g_rmaker_user_api_ctx.base_url, request_config->api_version ? request_config->api_version : g_rmaker_user_api_ctx.api_version, request_config->api_name, request_config->api_query_params);
        } else {
            snprintf(url, sizeof(url), "%s/%s/%s", g_rmaker_user_api_ctx.base_url, request_config->api_version ? request_config->api_version : g_rmaker_user_api_ctx.api_version, request_config->api_name);
        }
    }

    /* Initialize HTTP client */
    bool reuse_connection = false;
    esp_http_client_handle_t client = NULL;
    esp_http_client_config_t config = {
        .buffer_size_tx = APP_RMAKER_USER_API_HTTP_TX_BUFFER_SIZE,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    if (request_config->reuse_session) {
        if (g_rmaker_user_api_ctx.http_client) {
            ESP_LOGD(TAG, "Using persistent connection");
            reuse_connection = true;
            client = g_rmaker_user_api_ctx.http_client;
            esp_http_client_set_url(client, url);
            esp_http_client_set_method(client, app_rmaker_user_api_get_http_method(request_config->api_type));
        } else {
            config.url = url;
            config.method = app_rmaker_user_api_get_http_method(request_config->api_type);
            config.keep_alive_enable = true;
            config.keep_alive_idle = 10;
            config.keep_alive_interval = 5;
            config.keep_alive_count = 3;
            client = esp_http_client_init(&config);
            if (!client) {
                ESP_LOGE(TAG, "Failed to initialize HTTP client");
                err = ESP_FAIL;
                goto unlock;
            }
        }
    } else {
        ESP_LOGI(TAG, "Using new connection");
        config.url = url;
        config.method = app_rmaker_user_api_get_http_method(request_config->api_type);
        client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to initialize HTTP client");
            err = ESP_FAIL;
            goto unlock;
        }
    }

    /* Set HTTP headers */
    app_rmaker_user_api_set_http_headers(client, request_config->payload_is_json, !request_config->no_need_authorize);

    /* Make HTTP request */
    if (request_config->api_type == APP_RMAKER_USER_API_TYPE_POST
        || request_config->api_type == APP_RMAKER_USER_API_TYPE_PUT) {
        err = app_rmaker_user_api_make_http_request(client, request_config->api_payload, reuse_connection);
    } else {
        err = app_rmaker_user_api_make_http_request(client, NULL, reuse_connection);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to make HTTP request");
        if (reuse_connection) {
            ESP_LOGI(TAG, "Retrying with new connection");
            esp_http_client_cleanup(client);
            g_rmaker_user_api_ctx.http_client = NULL;
            return app_rmaker_user_api_generic(request_config, status_code, response_data);
        }
        goto exit;
    }

    /* Handle HTTP response */
    bool need_retry = false;
    err = app_rmaker_user_api_handle_http_response(client, status_code, response_data, &need_retry);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to handle HTTP response");
        goto exit;
    }

    if (!request_config->reuse_session) {
        esp_http_client_cleanup(client);
    } else {
        /* No need to cleanup, reuse the connection */
        g_rmaker_user_api_ctx.http_client = client;
    }
    if (need_retry) {
        app_rmaker_user_api_safe_free(response_data);
        *status_code = 0;
        return app_rmaker_user_api_generic(request_config, status_code, response_data);
    }
    goto unlock;
exit:
    esp_http_client_cleanup(client);
    if (reuse_connection) {
        /* If the connection is reused and the source is cleanup, set the global HTTP client to NULL */
        g_rmaker_user_api_ctx.http_client = NULL;
    }
unlock:
    APP_RMAKER_USER_API_UNLOCK();
    return err;
}
