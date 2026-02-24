/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "app_rmaker_user_api.h"
#include "app_rmaker_user_helper_api.h"
#include "protocol_examples_common.h"
#include "cJSON.h"

static const char* TAG = "rainmaker_api_example";

#define TEST_GROUP_NAME "test_group"

static void login_failure_callback(int error_code, const char *failed_reason)
{
    ESP_LOGI(TAG, "Login failed, error code: %d, reason: %s", error_code, failed_reason);
}

static void login_success_callback(void)
{
    ESP_LOGI(TAG, "Login success");
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    app_rmaker_user_api_config_t config = {
#if CONFIG_EXAMPLE_RAINMAKER_LOGIN_REFRESH_TOKEN
        .refresh_token = CONFIG_EXAMPLE_RAINMAKER_REFRESH_TOKEN,
#elif CONFIG_EXAMPLE_RAINMAKER_LOGIN_USERNAME_PASSWORD
        .username = CONFIG_EXAMPLE_RAINMAKER_USERNAME,
        .password = CONFIG_EXAMPLE_RAINMAKER_PASSWORD,
#endif
        .base_url = CONFIG_EXAMPLE_RAINMAKER_BASE_URL,
    };
    ret = app_rmaker_user_api_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init rainmaker api");
        return;
    }
    app_rmaker_user_api_register_login_failure_callback(login_failure_callback);
    app_rmaker_user_api_register_login_success_callback(login_success_callback);
    /* get user id */
    char *user_id = NULL;
    ret = app_rmaker_user_helper_api_get_user_id(&user_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get user id");
        app_rmaker_user_api_deinit();
        return;
    }
    ESP_LOGI(TAG, "user id: %s", user_id);
    free(user_id);
    /* create group */
    char *response_data = NULL;
    char *group_payload = "{\"group_name\":\"" TEST_GROUP_NAME "\"}";
    ret = app_rmaker_user_helper_api_create_group(group_payload, &response_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create group");
        if (response_data) {
            ESP_LOGE(TAG, "reason: %s", response_data);
        }
    } else {
        ESP_LOGI(TAG, "group created: %s", group_payload);
    }
    if (response_data) {
        free(response_data);
        response_data = NULL;
    }
    /* get groups */
    char *groups = NULL;
    ret = app_rmaker_user_helper_api_get_groups(NULL, &groups);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get all groups");
        app_rmaker_user_api_deinit();
        return;
    }
    ESP_LOGI(TAG, "groups: %s", groups);
    /* get test_group id */
    char *test_group_id = NULL;
    cJSON *groups_json = cJSON_Parse(groups);
    free(groups);
    if (groups_json == NULL) {
        ESP_LOGE(TAG, "Failed to parse groups json");
        app_rmaker_user_api_deinit();
        return;
    }
    cJSON *groups_array = cJSON_GetObjectItem(groups_json, "groups");
    if (groups_array && cJSON_IsArray(groups_array)) {
        int groups_total = cJSON_GetArraySize(groups_array);
        for (int i = 0; i < groups_total; i++) {
            cJSON *group = cJSON_GetArrayItem(groups_array, i);
            if (group) {
                cJSON *group_name = cJSON_GetObjectItem(group, "group_name");
                if (group_name && cJSON_IsString(group_name) && strcmp(group_name->valuestring, TEST_GROUP_NAME) == 0) {
                    cJSON *group_id = cJSON_GetObjectItem(group, "group_id");
                    if (group_id && cJSON_IsString(group_id)) {
                        test_group_id = strdup(group_id->valuestring);
                        break;
                    }
                }
            }
        }
    }
    cJSON_Delete(groups_json);
    if (!test_group_id) {
        ESP_LOGE(TAG, "test group not found");
        app_rmaker_user_api_deinit();
        return;
    }
    ESP_LOGI(TAG, "test group_id: %s", test_group_id);
    /* get node list */
    char *node_list = NULL;
    uint16_t node_count = 0;
    ret = app_rmaker_user_helper_api_get_nodes_list(&node_list, &node_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get node list");
        app_rmaker_user_api_deinit();
        free(test_group_id);
        return;
    }
    ESP_LOGI(TAG, "node list: %s, count %d", node_list, node_count);
    cJSON* node_list_json = cJSON_Parse(node_list);
    free(node_list);
    if (node_list_json == NULL) {
        ESP_LOGE(TAG, "Failed to parse node list");
        app_rmaker_user_api_deinit();
        free(test_group_id);
        return;
    }

    for (int i = 0; i < node_count; i++) {
        cJSON* node = cJSON_GetArrayItem(node_list_json, i);
        ESP_LOGI(TAG, "Get node config for %s", node->valuestring);
        /* get node config */
        char* node_config = NULL;
        ret = app_rmaker_user_helper_api_get_node_config(node->valuestring, &node_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get node %s config", node->valuestring);
            continue;
        }
        ESP_LOGI(TAG, "node %s config: %s", node->valuestring, node_config);
        cJSON* node_config_json = cJSON_Parse(node_config);
        free(node_config);
        if (node_config_json == NULL) {
            ESP_LOGE(TAG, "Failed to parse node %s config", node->valuestring);
            continue;
        }
        /* get node device name */
        char* device_name = NULL;
        cJSON* devices = cJSON_GetObjectItem(node_config_json, "devices");
        if (devices != NULL) {
            /* get first device name */
            cJSON* device = cJSON_GetArrayItem(devices, 0);
            if (device != NULL) {
                cJSON* name = cJSON_GetObjectItem(device, "name");
                if (name != NULL) {
                    device_name = strdup(name->valuestring);
                }
            }
        }
        cJSON_Delete(node_config_json);
        /* get node connection status */
        bool connection_status = false;
        ret = app_rmaker_user_helper_api_get_node_connection_status(node->valuestring, &connection_status);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get node %s connection status", node->valuestring);
        } else {
            ESP_LOGI(TAG, "node %s connection status: %s", node->valuestring, connection_status ? "connected" : "disconnected");
        }
        /* get node parameters */
        char* node_params = NULL;
        ret = app_rmaker_user_helper_api_get_node_params(node->valuestring, &node_params);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get node %s params", node->valuestring);
            continue;
        }
        ESP_LOGI(TAG, "node %s params: %s", node->valuestring, node_params);
        cJSON* node_params_json = cJSON_Parse(node_params);
        free(node_params);
        if (node_params_json == NULL) {
            ESP_LOGE(TAG, "Failed to parse node %s params", node->valuestring);
            if (device_name != NULL) {
                free(device_name);
            }
            continue;
        }
        /* check if has power parameter */
        bool has_power = false;
        bool power_value = false;
        cJSON* power = cJSON_GetObjectItem(node_params_json, device_name);
        if (power != NULL) {
            has_power = true;
            cJSON* power_value_json = cJSON_GetObjectItem(power, "Power");
            if (power_value_json != NULL) {
                power_value = power_value_json->valueint;
            }
        }
        cJSON_Delete(node_params_json);
        /* set node parameters */
        if (has_power && device_name != NULL) {
            char node_set_params[128] = {0};
            snprintf(node_set_params, sizeof(node_set_params), "[{\"node_id\":\"%s\",\"payload\":{\"%s\":{\"Power\":%s}}}]", node->valuestring, device_name, !power_value ? "true" : "false");
            ret = app_rmaker_user_helper_api_set_node_params(NULL, node_set_params, &response_data);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set node parameters");
                if (response_data) {
                    ESP_LOGE(TAG, "reason: %s", response_data);
                }
            }
            if (response_data) {
                free(response_data);
                response_data = NULL;
            }
        }
        if (device_name != NULL) {
            free(device_name);
        }
        /* add node to group */
        char group_operation_payload[128] = {0};
        snprintf(group_operation_payload, sizeof(group_operation_payload), "{\"group_name\":\"%s\",\"operation\":\"add\",\"nodes\":[\"%s\"]}", TEST_GROUP_NAME, node->valuestring);
        ret = app_rmaker_user_helper_api_operate_node_to_group(test_group_id, group_operation_payload, &response_data);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add node %s to group %s", node->valuestring, TEST_GROUP_NAME);
            if (response_data) {
                ESP_LOGE(TAG, "reason: %s", response_data);
            }
        } else {
            ESP_LOGI(TAG, "Node %s added to group %s", node->valuestring, TEST_GROUP_NAME);
        }
        if (response_data) {
            free(response_data);
            response_data = NULL;
        }
    }
    cJSON_Delete(node_list_json);
    /* get group nodes */
    char *group_nodes = NULL;
    ret = app_rmaker_user_helper_api_get_groups(test_group_id, &group_nodes);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get group %s nodes", test_group_id);
    } else {
        ESP_LOGI(TAG, "group %s nodes: %s", test_group_id, group_nodes);
    }
    free(group_nodes);
    /* delete group */
    ret = app_rmaker_user_helper_api_delete_group(test_group_id, &response_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete group");
        if (response_data) {
            ESP_LOGE(TAG, "reason: %s", response_data);
        }
    } else {
        ESP_LOGI(TAG, "group deleted: %s", test_group_id);
    }
    if (response_data) {
        free(response_data);
        response_data = NULL;
    }
    free(test_group_id);
    /* get nodes */
    app_rmaker_user_api_request_config_t request_config = {
        .reuse_session = true,
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = "user/nodes",
        .api_version = "v1",
        .api_query_params = "node_details=true&status=true&config=true&params=true&show_tags=true&is_matter=false",
        .api_payload = NULL,
    };
    int status_code = 0;
    ret = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get nodes");
    } else {
        ESP_LOGI(TAG, "status code: %d, nodes: %s", status_code, response_data);
    }
    if (response_data) {
        free(response_data);
        response_data = NULL;
    }
    app_rmaker_user_api_deinit();
}
