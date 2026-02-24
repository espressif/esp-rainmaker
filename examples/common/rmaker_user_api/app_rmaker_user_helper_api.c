/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* RainMaker User API helper layer (user, nodes, groups, node mapping, etc.) */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <esp_log.h>
#include <cJSON.h>
#include "app_rmaker_user_api.h"
#include "app_rmaker_user_helper_api.h"
#include "app_rmaker_user_api_util.h"

static const char *TAG = "rmaker_user_helper_api";
static const char *app_rmaker_user_api_info_url = "user2";
static const char *app_rmaker_user_api_nodes_url = "user/nodes";
static const char *app_rmaker_user_api_nodes_params_url = "user/nodes/params";
static const char *app_rmaker_user_api_nodes_config_url = "user/nodes/config";
static const char *app_rmaker_user_api_nodes_group_url = "user/node_group";
static const char *app_rmaker_user_api_nodes_mapping_url = "user/nodes/mapping";
static const char *app_rmaker_user_api_node_mapping_add_payload = "{\"secret_key\":\"%s\",\"node_id\":\"%s\",\"operation\":\"add\"}";
static const char *app_rmaker_user_api_node_mapping_remove_payload = "{\"node_id\":\"%s\",\"operation\":\"remove\"}";

/**
 * @brief Parse user info response
 */
static esp_err_t app_rmaker_user_api_parse_user_info_response(const char *response_data, char **user_id)
{
    esp_err_t err = ESP_FAIL;
    cJSON *response = cJSON_Parse(response_data);
    if (response) {
        cJSON *user_id_json = cJSON_GetObjectItem(response, "user_id");
        if (user_id_json && user_id_json->valuestring) {
            *user_id = app_rmaker_user_api_safe_strdup(user_id_json->valuestring);
            err = ESP_OK;
        } else {
            ESP_LOGE(TAG, "No user ID found in response");
        }
        cJSON_Delete(response);
    } else {
        ESP_LOGE(TAG, "Failed to parse user info response");
    }
    return err;
}

/**
 * @brief Parse nodes list response
 */
static esp_err_t app_rmaker_user_api_parse_nodes_list_response(const char *response_data, char **nodes_list, uint16_t *nodes_count)
{
    esp_err_t err = ESP_FAIL;
    *nodes_list = NULL;
    *nodes_count = 0;
    cJSON *response = cJSON_Parse(response_data);
    if (!response) {
        ESP_LOGE(TAG, "Failed to parse nodes list response JSON");
        return err;
    }

    /* Get nodes array */
    cJSON *nodes_json = cJSON_GetObjectItem(response, "nodes");
    if (nodes_json && cJSON_IsArray(nodes_json)) {
        /* Convert nodes array to JSON string */
        char *nodes_array_str = cJSON_PrintUnformatted(nodes_json);
        if (nodes_array_str) {
            *nodes_list = nodes_array_str; /* Caller must free this */
            /* Get total count */
            cJSON *total_json = cJSON_GetObjectItem(response, "total");
            if (total_json && cJSON_IsNumber(total_json)) {
                *nodes_count = (uint16_t)total_json->valueint;
                err = ESP_OK;
            } else {
                ESP_LOGE(TAG, "No total field found in response");
                app_rmaker_user_api_safe_free(nodes_list);
                *nodes_list = NULL;
            }
        } else {
            ESP_LOGE(TAG, "Failed to convert nodes array to string");
        }
    } else {
        ESP_LOGE(TAG, "No nodes array found in response");
    }

    cJSON_Delete(response);
    return err;
}

/**
 * @brief Parse node config response
 */
static esp_err_t app_rmaker_user_api_parse_node_config_response(const char *response_data, char **node_config)
{
    esp_err_t err = ESP_FAIL;
    *node_config = NULL;
    cJSON *response = cJSON_Parse(response_data);
    if (response) {
        char *node_config_str = cJSON_PrintUnformatted(response);
        if (node_config_str) {
            *node_config = node_config_str;
            err = ESP_OK;
        } else {
            ESP_LOGE(TAG, "Failed to convert node config to string");
        }
        cJSON_Delete(response);
    } else {
        ESP_LOGE(TAG, "Failed to parse node config response");
    }
    return err;
}

/**
 * @brief Parse node params response
 */
static esp_err_t app_rmaker_user_api_parse_node_params_response(const char *response_data, char **node_params)
{
    esp_err_t err = ESP_FAIL;
    *node_params = NULL;
    cJSON *response = cJSON_Parse(response_data);
    if (response) {
        char *node_params_str = cJSON_PrintUnformatted(response);
        if (node_params_str) {
            *node_params = node_params_str;
            err = ESP_OK;
        } else {
            ESP_LOGE(TAG, "Failed to convert node params to string");
        }
        cJSON_Delete(response);
    } else {
        ESP_LOGE(TAG, "Failed to parse node params response");
    }
    return err;
}

/**
 * @brief Parse group response
 */
static esp_err_t app_rmaker_user_api_parse_group_response(const char *response_data, char **groups)
{
    esp_err_t err = ESP_FAIL;
    *groups = NULL;
    cJSON *response = cJSON_Parse(response_data);
    if (response) {
        char *group_str = cJSON_PrintUnformatted(response);
        if (group_str) {
            *groups = group_str;
            err = ESP_OK;
        } else {
            ESP_LOGE(TAG, "Failed to convert groups to string");
        }
        cJSON_Delete(response);
    } else {
        ESP_LOGE(TAG, "Failed to parse groups response");
    }
    return err;
}

/**
 * @brief Parse node mapping response
 */
static esp_err_t app_rmaker_user_api_parse_node_mapping_response(const char *response_data, char **request_id, app_rmaker_user_helper_api_node_mapping_operation_type_t operation_type)
{
    esp_err_t err = ESP_FAIL;
    cJSON *response = cJSON_Parse(response_data);
    if (response) {
        cJSON *status_json = cJSON_GetObjectItem(response, "status");
        if (status_json && status_json->valuestring) {
            if (strcmp(status_json->valuestring, "success") == 0) {
                if (operation_type == APP_RMAKER_USER_HELPER_API_ADD_NODE_MAPPING) {
                    cJSON *request_id_json = cJSON_GetObjectItem(response, "request_id");
                    if (request_id_json && request_id_json->valuestring) {
                        *request_id = app_rmaker_user_api_safe_strdup(request_id_json->valuestring);
                        err = ESP_OK;
                    }
                } else if (operation_type == APP_RMAKER_USER_HELPER_API_REMOVE_NODE_MAPPING) {
                    err = ESP_OK;
                }
            } else {
                ESP_LOGE(TAG, "Failed to set node mapping, status: %s", status_json->valuestring);
            }
        }
        cJSON_Delete(response);
    } else {
        ESP_LOGE(TAG, "Failed to parse node mapping response");
    }
    return err;
}

/**
 * @brief Parse node mapping status response
 */
static esp_err_t app_rmaker_user_api_parse_node_mapping_status_response(const char *response_data, app_rmaker_user_helper_api_node_mapping_status_type_t *status)
{
    esp_err_t err = ESP_FAIL;
    *status = APP_RMAKER_USER_HELPER_API_NODE_MAPPING_STATUS_INTERNAL_ERROR;
    cJSON *response = cJSON_Parse(response_data);
    if (response) {
        cJSON *status_json = cJSON_GetObjectItem(response, "request_status");
        if (status_json && status_json->valuestring) {
            if (strcmp(status_json->valuestring, "requested") == 0) {
                *status = APP_RMAKER_USER_HELPER_API_NODE_MAPPING_STATUS_REQUESTED;
            } else if (strcmp(status_json->valuestring, "confirmed") == 0) {
                *status = APP_RMAKER_USER_HELPER_API_NODE_MAPPING_STATUS_CONFIRMED;
            } else if (strcmp(status_json->valuestring, "timeout") == 0) {
                *status = APP_RMAKER_USER_HELPER_API_NODE_MAPPING_STATUS_TIMEOUT;
            } else if (strcmp(status_json->valuestring, "discarded") == 0) {
                *status = APP_RMAKER_USER_HELPER_API_NODE_MAPPING_STATUS_DISCARDED;
            } else {
                ESP_LOGE(TAG, "Unknown node mapping status: %s", status_json->valuestring);
            }
            err = ESP_OK;
        }
        cJSON_Delete(response);
    } else {
        ESP_LOGE(TAG, "Failed to parse node mapping status response");
    }
    return err;
}

/**
 * @brief Parse node connection status response
 */
static esp_err_t app_rmaker_user_api_parse_node_connection_status_response(const char *response_data, bool *connection_status)
{
    *connection_status = false;
    cJSON *response = cJSON_Parse(response_data);
    if (response) {
        cJSON *node_details = cJSON_GetObjectItem(response, "node_details");
        if (!node_details || !cJSON_IsArray(node_details)) {
            ESP_LOGE(TAG, "No node_details object in response");
            cJSON_Delete(response);
            return ESP_ERR_INVALID_RESPONSE;
        }

        cJSON *node = cJSON_GetArrayItem(node_details, 0);
        if (!node) {
            ESP_LOGE(TAG, "No node object in node_details array");
            cJSON_Delete(response);
            return ESP_ERR_INVALID_RESPONSE;
        }

        cJSON *status = cJSON_GetObjectItem(node, "status");
        if (!status) {
            ESP_LOGE(TAG, "No status object in node");
            cJSON_Delete(response);
            return ESP_ERR_INVALID_RESPONSE;
        }

        cJSON *connectivity = cJSON_GetObjectItem(status, "connectivity");
        if (!connectivity) {
            ESP_LOGE(TAG, "No connectivity object in status");
            cJSON_Delete(response);
            return ESP_ERR_INVALID_RESPONSE;
        }

        cJSON *connected = cJSON_GetObjectItem(connectivity, "connected");
        if (!connected || !cJSON_IsBool(connected)) {
            ESP_LOGE(TAG, "No connected field in connectivity");
            cJSON_Delete(response);
            return ESP_ERR_INVALID_RESPONSE;
        }

        if (connection_status) {
            *connection_status = cJSON_IsTrue(connected);
        }
        cJSON_Delete(response);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to parse node connection status response");
    }
    return ESP_FAIL;
}

/* GET /{version}/user2 */
esp_err_t app_rmaker_user_helper_api_get_user_id(char **user_id)
{
    if (!user_id) {
        ESP_LOGE(TAG, "User ID is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    *user_id = NULL;
    char *response_data = NULL;
    int status_code = 0;

    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = app_rmaker_user_api_info_url,
        .api_query_params = NULL,
        .api_payload = NULL,
    };

    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to execute get user ID request");
        return err;
    }

    /* Parse response and update user id */
    err = app_rmaker_user_api_parse_user_info_response(response_data, user_id);
    app_rmaker_user_api_safe_free(&response_data);
    return err;
}

/* GET /{version}/user/nodes */
esp_err_t app_rmaker_user_helper_api_get_nodes_list(char **nodes_list, uint16_t *nodes_count)
{
    if (!nodes_list || !nodes_count) {
        ESP_LOGE(TAG, "Nodes list or nodes count is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    *nodes_list = NULL;
    *nodes_count = 0;
    char *response_data = NULL;
    int status_code = 0;

    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = app_rmaker_user_api_nodes_url,
        .api_query_params = "node_details=false&status=false&config=false&params=false&show_tags=false&is_matter=false",
        .api_payload = NULL,
    };

    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to execute get nodes list request");
        return err;
    }

    err = app_rmaker_user_api_parse_nodes_list_response(response_data, nodes_list, nodes_count);
    app_rmaker_user_api_safe_free(&response_data);
    return err;
}

/* GET /{version}/user/nodes/config */
esp_err_t app_rmaker_user_helper_api_get_node_config(const char *node_id, char **node_config)
{
    if (!node_id || !node_config) {
        ESP_LOGE(TAG, "Node ID or node config is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    *node_config = NULL;
    char *response_data = NULL;
    int status_code = 0;

    char query_params[64] = {0};
    snprintf(query_params, sizeof(query_params), "node_id=%s", node_id);
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = app_rmaker_user_api_nodes_config_url,
        .api_query_params = query_params,
        .api_payload = NULL,
    };

    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to execute get node config request");
        return err;
    }

    err = app_rmaker_user_api_parse_node_config_response(response_data, node_config);
    app_rmaker_user_api_safe_free(&response_data);
    return err;
}

/* PUT /{version}/user/nodes/params */
esp_err_t app_rmaker_user_helper_api_set_node_params(const char *node_id, const char *payload, char **response_data)
{
    if (!payload || !response_data) {
        ESP_LOGE(TAG, "Payload or response data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    *response_data = NULL;
    int status_code = 0;

    char query_params[64] = {0};
    if (node_id) {
        snprintf(query_params, sizeof(query_params), "node_id=%s", node_id);
    }
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_PUT,
        .api_name = app_rmaker_user_api_nodes_params_url,
        .api_query_params = strlen(query_params) > 0 ? query_params : NULL,
        .api_payload = payload,
    };

    return app_rmaker_user_api_generic(&request_config, &status_code, response_data);
}

/* GET /{version}/user/nodes/params */
esp_err_t app_rmaker_user_helper_api_get_node_params(const char *node_id, char **node_params)
{
    if (!node_id || !node_params) {
        ESP_LOGE(TAG, "Node ID or node params is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    *node_params = NULL;
    char *response_data = NULL;
    int status_code = 0;

    char query_params[64] = {0};
    snprintf(query_params, sizeof(query_params), "node_id=%s", node_id);
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = app_rmaker_user_api_nodes_params_url,
        .api_query_params = query_params,
        .api_payload = NULL,
    };

    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to execute get node params request");
        return err;
    }

    /* Parse response and update node params */
    err = app_rmaker_user_api_parse_node_params_response(response_data, node_params);
    app_rmaker_user_api_safe_free(&response_data);
    return err;
}

/* GET /{version}/user/node_group */
esp_err_t app_rmaker_user_helper_api_get_groups(const char *group_id, char **groups)
{
    if (!groups) {
        ESP_LOGE(TAG, "Groups is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    *groups = NULL;
    char *response_data = NULL;
    int status_code = 0;

    char query_params[64] = {0};
    if (group_id) {
        snprintf(query_params, sizeof(query_params), "group_id=%s&node_list=true", group_id);
    } else {
        snprintf(query_params, sizeof(query_params), "node_list=true");
    }
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = app_rmaker_user_api_nodes_group_url,
        .api_query_params = query_params,
        .api_payload = NULL,
    };
    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to execute get group request");
        return err;
    }

    /* Parse response and update group */
    err = app_rmaker_user_api_parse_group_response(response_data, groups);
    app_rmaker_user_api_safe_free(&response_data);
    return err;
}

/* POST /{version}/user/node_group */
esp_err_t app_rmaker_user_helper_api_create_group(const char *group_payload, char **response_data)
{
    if (!group_payload || !response_data) {
        ESP_LOGE(TAG, "Group payload or response data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    *response_data = NULL;
    int status_code = 0;

    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_POST,
        .api_name = app_rmaker_user_api_nodes_group_url,
        .api_query_params = NULL,
        .api_payload = group_payload,
    };

    return app_rmaker_user_api_generic(&request_config, &status_code, response_data);
}

/* DELETE /{version}/user/node_group */
esp_err_t app_rmaker_user_helper_api_delete_group(const char *group_id, char **response_data)
{
    if (!group_id || !response_data) {
        ESP_LOGE(TAG, "Group ID or response data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    *response_data = NULL;
    int status_code = 0;

    char query_params[64] = {0};
    snprintf(query_params, sizeof(query_params), "group_id=%s", group_id);
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_DELETE,
        .api_name = app_rmaker_user_api_nodes_group_url,
        .api_query_params = query_params,
        .api_payload = NULL,
    };

    return app_rmaker_user_api_generic(&request_config, &status_code, response_data);
}

/* POST /{version}/user/node_group (operate nodes) */
esp_err_t app_rmaker_user_helper_api_operate_node_to_group(const char *group_id, const char *group_payload, char **response_data)
{
    if (!group_id || !group_payload || !response_data) {
        ESP_LOGE(TAG, "Group ID or Group payload or response data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    *response_data = NULL;
    char query_params[64] = {0};
    int status_code = 0;
    snprintf(query_params, sizeof(query_params), "group_id=%s", group_id);
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_PUT,
        .api_name = app_rmaker_user_api_nodes_group_url,
        .api_query_params = query_params,
        .api_payload = group_payload,
    };

    return app_rmaker_user_api_generic(&request_config, &status_code, response_data);
}

/* POST /{version}/user/nodes/mapping */
esp_err_t app_rmaker_user_helper_api_set_node_mapping(const char *secret_key, const char *node_id, app_rmaker_user_helper_api_node_mapping_operation_type_t operation_type, char **request_id)
{
    char payload[256] = {0};
    if (operation_type == APP_RMAKER_USER_HELPER_API_REMOVE_NODE_MAPPING) {
        if (!node_id) {
            ESP_LOGE(TAG, "Node ID is required for remove node mapping");
            return ESP_ERR_INVALID_ARG;
        }
        snprintf(payload, sizeof(payload), app_rmaker_user_api_node_mapping_remove_payload, node_id);
    } else if (operation_type == APP_RMAKER_USER_HELPER_API_ADD_NODE_MAPPING) {
        if (!secret_key || !node_id || !request_id) {
            ESP_LOGE(TAG, "Secret key, node ID or request ID is required for add node mapping");
            return ESP_ERR_INVALID_ARG;
        }
        snprintf(payload, sizeof(payload), app_rmaker_user_api_node_mapping_add_payload, secret_key, node_id);
        *request_id = NULL;
    } else {
        ESP_LOGE(TAG, "Invalid operation type");
        return ESP_ERR_INVALID_ARG;
    }

    int status_code = 0;
    char *response_data = NULL;

    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_PUT,
        .api_name = app_rmaker_user_api_nodes_mapping_url,
        .api_query_params = NULL,
        .api_payload = payload,
    };

    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to execute set node mapping request");
        return err;
    }

    err = app_rmaker_user_api_parse_node_mapping_response(response_data, request_id, operation_type);
    app_rmaker_user_api_safe_free(&response_data);
    return err;
}

/* GET /{version}/user/nodes/mapping (status) */
esp_err_t app_rmaker_user_helper_api_get_node_mapping_status(const char *request_id, app_rmaker_user_helper_api_node_mapping_status_type_t *status)
{
    *status = APP_RMAKER_USER_HELPER_API_NODE_MAPPING_STATUS_INTERNAL_ERROR;
    if (!request_id) {
        ESP_LOGE(TAG, "Request ID is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    char query_params[64] = {0};
    int status_code = 0;
    char *response_data = NULL;

    snprintf(query_params, sizeof(query_params), "request_id=%s", request_id);
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = app_rmaker_user_api_nodes_mapping_url,
        .api_query_params = query_params,
        .api_payload = NULL,
    };

    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to execute get node mapping status request");
        return err;
    }

    /* Parse response and update status */
    err = app_rmaker_user_api_parse_node_mapping_status_response(response_data, status);
    app_rmaker_user_api_safe_free(&response_data);
    return err;
}

/* GET /{version}/user/nodes (connection status) */
esp_err_t app_rmaker_user_helper_api_get_node_connection_status(const char *node_id, bool *connection_status)
{
    if (!node_id || !connection_status) {
        ESP_LOGE(TAG, "Node ID or connection_status is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    *connection_status = false;
    int status_code = 0;
    char *response_data = NULL;

    char query_params[128] = {0};
    snprintf(query_params, sizeof(query_params), "node_id=%s&node_details=false&status=true&config=false&params=false&show_tags=false&is_matter=false", node_id);
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = app_rmaker_user_api_nodes_url,
        .api_query_params = query_params,
        .api_payload = NULL,
    };

    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to execute get node connection status request");
        return err;
    }

    /* Parse response and update connection status */
    err = app_rmaker_user_api_parse_node_connection_status_response(response_data, connection_status);
    app_rmaker_user_api_safe_free(&response_data);
    return err;
}
