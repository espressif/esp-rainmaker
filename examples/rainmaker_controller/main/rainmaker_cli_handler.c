/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* RainMaker CLI handler: wraps User API for console commands */

#include "esp_log.h"
#include "rainmaker_cli_handler.h"
#include "app_rmaker_user_api.h"
#include "app_rmaker_user_helper_api.h"
#include "esp_rmaker_auth_service.h"

static const char *TAG = "rainmaker_cli_handler";

static void login_failure_callback(int error_code, const char *failed_reason)
{
    esp_rmaker_user_auth_service_token_status_update(ESP_RMAKER_USER_AUTH_SERVICE_TOKEN_STATUS_EXPIRED_OR_INVALID);
    ESP_LOGE(TAG, "Login failed, error code: %d, reason: %s", error_code, failed_reason);
}

static void login_success_callback(void)
{
    esp_rmaker_user_auth_service_token_status_update(ESP_RMAKER_USER_AUTH_SERVICE_TOKEN_STATUS_VERIFIED);
    ESP_LOGI(TAG, "Login success");
}

esp_err_t rainmaker_cli_handler_init(const char *refresh_token, const char *base_url)
{
    if (refresh_token == NULL || base_url == NULL) {
        ESP_LOGE(TAG, "Refresh token or base URL is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    app_rmaker_user_api_config_t config = {
        .base_url = (char *)base_url,
        .refresh_token = (char *)refresh_token,
    };
    app_rmaker_user_api_register_login_failure_callback(login_failure_callback);
    app_rmaker_user_api_register_login_success_callback(login_success_callback);
    return app_rmaker_user_api_init(&config);
}

esp_err_t rainmaker_cli_handler_get_nodes_list(char **nodes_list, uint16_t *nodes_count)
{
    return app_rmaker_user_helper_api_get_nodes_list(nodes_list, nodes_count);
}

esp_err_t rainmaker_cli_handler_get_node_details(char **node_details, int *status_code)
{
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = "user/nodes",
        .api_version = "v1",
        .api_query_params = "node_details=true&status=true&config=true&params=true&show_tags=true&is_matter=false",
        .api_payload = NULL,
    };
    return app_rmaker_user_api_generic(&request_config, status_code, node_details);
}

esp_err_t rainmaker_cli_handler_get_node_params(const char *node_id, char **node_params)
{
    return app_rmaker_user_helper_api_get_node_params(node_id, node_params);
}

esp_err_t rainmaker_cli_handler_set_node_params(const char *node_id, const char *payload, char **response_data)
{
    return app_rmaker_user_helper_api_set_node_params(node_id, payload, response_data);
}

esp_err_t rainmaker_cli_handler_get_node_config(const char *node_id, char **node_config)
{
    return app_rmaker_user_helper_api_get_node_config(node_id, node_config);
}

esp_err_t rainmaker_cli_handler_get_node_connection_status(const char *node_id, bool *connection_status)
{
    return app_rmaker_user_helper_api_get_node_connection_status(node_id, connection_status);
}

esp_err_t rainmaker_cli_handler_remove_node(const char *node_id, char **response_data)
{
    if (node_id == NULL || response_data == NULL) {
        ESP_LOGE(TAG, "Node ID or response data is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    char payload[256] = {0};
    snprintf(payload, sizeof(payload), "{\"node_id\":\"%s\",\"operation\":\"remove\"}", node_id);
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_PUT,
        .api_name = "user/nodes/mapping",
        .api_version = "v1",
        .api_query_params = NULL,
        .api_payload = (char *)payload,
    };
    int status_code = 0;
    return app_rmaker_user_api_generic(&request_config, &status_code, response_data);
}
