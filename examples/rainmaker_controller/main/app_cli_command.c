/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* RainMaker controller CLI: console commands for nodes, params, schedules, etc. */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <cJSON.h>
#include <nvs_flash.h>
#include <esp_console.h>
#include <sdkconfig.h>
#include <esp_log.h>
#include <esp_system.h>
#include <argtable3/argtable3.h>
#include "rainmaker_cli_handler.h"
#include "esp_rmaker_controller.h"
#include "esp_rmaker_auth_service.h"

static const char *TAG = "rainmaker_controller_cli";
static bool is_rainmaker_api_initialized = false;

#define PROMPT_STR              "rainmaker_controller_cli>"
#define CONSOLE_PRINTF(fmt, ...) printf("%s " fmt, PROMPT_STR, ##__VA_ARGS__)

#define CMD_GET_NODES          "getnodes"
#define CMD_GET_NODE_DETAILS   "getnodedetails"
#define CMD_GET_SCHEDULES      "getschedules"
#define CMD_SET_SCHEDULE       "setschedule"
#define CMD_GET_NODE_CONFIG    "getnodeconfig"
#define CMD_GET_NODE_STATUS    "getnodestatus"
#define CMD_SET_NODE_PARAMS    "setparams"
#define CMD_GET_NODE_PARAMS    "getparams"
#define CMD_REMOVE_NODE        "removenode"
#define CMD_GET_HEAP_STATUS    "getheapstatus"

/* ---------- Command handlers ---------- */

static int get_nodes_command_handler(int argc, char **argv)
{
    char *nodes_list = NULL;
    uint16_t nodes_count = 0;
    if (rainmaker_cli_handler_get_nodes_list(&nodes_list, &nodes_count) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get nodes list");
        if (nodes_list) {
            free(nodes_list);
        }
        return ESP_FAIL;
    }
    /* nodes_list is a JSON array string: ["id1","id2",...] */
    if (nodes_list) {
        cJSON *nodes_json = cJSON_Parse(nodes_list);
        free(nodes_list);
        if (nodes_json && cJSON_IsArray(nodes_json)) {
            int arr_size = cJSON_GetArraySize(nodes_json);
            printf("\r\n");
            for (int i = 0; i < arr_size; ++i) {
                cJSON *item = cJSON_GetArrayItem(nodes_json, i);
                if (cJSON_IsString(item) && item->valuestring) {
                    printf("%d. %s\n", i + 1, item->valuestring);
                }
            }
            printf("\r\n");
        }
        if (nodes_json) {
            cJSON_Delete(nodes_json);
        }
    }
    return ESP_OK;
}

static int get_node_details_command_handler(int argc, char **argv)
{
    int ret = ESP_OK;
    char *node_details = NULL;
    int status_code = 0;
    if (rainmaker_cli_handler_get_node_details(&node_details, &status_code) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get node details, status code: %d", status_code);
        ret = ESP_FAIL;
    }
    if (node_details) {
        printf("%s\n", node_details);
        free(node_details);
    }
    return ret;
}

static int get_schedules_command_handler(int argc, char **argv)
{
    if (argc < 2 || !argv[1]) {
        ESP_LOGE(TAG, "Node ID is required");
        return ESP_FAIL;
    }
    char *node_params = NULL;
    if (rainmaker_cli_handler_get_node_params(argv[1], &node_params) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get node params");
        if (node_params) {
            free(node_params);
        }
        return ESP_FAIL;
    }
    if (node_params) {
        cJSON *node_params_json = cJSON_Parse(node_params);
        if (node_params_json && cJSON_IsObject(node_params_json)) {
            cJSON *schedule_json = cJSON_GetObjectItem(node_params_json, "Schedule");
            if (schedule_json) {
                char *schedules = cJSON_PrintUnformatted(schedule_json);
                if (schedules) {
                    printf("\r\n%s\r\n", schedules);
                    free(schedules);
                }
            } else {
                char *error_json = cJSON_PrintUnformatted(node_params_json);
                if (error_json) {
                    printf("\r\n%s\r\n", error_json);
                    free(error_json);
                }
            }
        } else {
            printf("Failed to parse node params\r\n");
        }
        if (node_params_json) {
            cJSON_Delete(node_params_json);
        }
        free(node_params);
    } else {
        ESP_LOGE(TAG, "Node params is NULL");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static int set_schedules_command_handler(int argc, char **argv)
{
    if (argc < 3 || !argv[1] || !argv[2]) {
        ESP_LOGE(TAG, "Node ID and schedule payload are required");
        return ESP_FAIL;
    }
    int ret = ESP_OK;
    char *response_data = NULL;
    if (rainmaker_cli_handler_set_node_params(argv[1], argv[2], &response_data) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set node parameters");
        ret = ESP_FAIL;
    }
    if (response_data) {
        printf("%s\n", response_data);
        free(response_data);
    }
    return ret;
}

static int get_node_config_command_handler(int argc, char **argv)
{
    if (argc < 2 || !argv[1]) {
        ESP_LOGE(TAG, "Node ID is required");
        return ESP_FAIL;
    }
    char *node_config = NULL;
    int ret = ESP_OK;
    if (rainmaker_cli_handler_get_node_config(argv[1], &node_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get node config");
        ret = ESP_FAIL;
    }
    if (node_config) {
        printf("%s\n", node_config);
        free(node_config);
    }
    return ret;
}

static int get_node_status_command_handler(int argc, char **argv)
{
    if (argc < 2 || !argv[1]) {
        ESP_LOGE(TAG, "Node ID is required");
        return ESP_FAIL;
    }
    bool connection_status = false;
    if (rainmaker_cli_handler_get_node_connection_status(argv[1], &connection_status) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get node connection status");
        return ESP_FAIL;
    }
    printf("Node %s is %s\n", argv[1], connection_status ? "online" : "offline");
    return ESP_OK;
}

static int set_node_params_command_handler(int argc, char **argv)
{
    if (argc < 3 || !argv[1] || !argv[2]) {
        ESP_LOGE(TAG, "Node ID and payload are required");
        return ESP_FAIL;
    }
    char *response_data = NULL;
    int ret = ESP_OK;
    if (rainmaker_cli_handler_set_node_params(argv[1], argv[2], &response_data) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set node parameters");
        ret = ESP_FAIL;
    }
    if (response_data) {
        printf("%s\n", response_data);
        free(response_data);
    }
    return ret;
}

static int get_node_params_command_handler(int argc, char **argv)
{
    if (argc < 2 || !argv[1]) {
        ESP_LOGE(TAG, "Node ID is required");
        return ESP_FAIL;
    }
    char *node_params = NULL;
    int ret = ESP_OK;
    if (rainmaker_cli_handler_get_node_params(argv[1], &node_params) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get node params");
        ret = ESP_FAIL;
    }
    if (node_params) {
        printf("%s\n", node_params);
        free(node_params);
    }
    return ret;
}

static int remove_node_command_handler(int argc, char **argv)
{
    if (argc < 2 || !argv[1]) {
        ESP_LOGE(TAG, "Node ID is required");
        return ESP_FAIL;
    }
    char *response_data = NULL;
    int ret = ESP_OK;
    if (rainmaker_cli_handler_remove_node(argv[1], &response_data) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to remove node");
        ret = ESP_FAIL;
    }
    if (response_data) {
        printf("%s\n", response_data);
        free(response_data);
    }
    return ret;
}

static int get_heap_status_command_handler(int argc, char **argv)
{
    CONSOLE_PRINTF("Heap size: %" PRIu32 ", Minimum heap size: %" PRIu32 "\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
    return ESP_OK;
}

/* ---------- Dispatcher ---------- */

static int app_cli_command_handler(int argc, char **argv)
{
    if (!is_rainmaker_api_initialized) {
        ESP_LOGD(TAG, "Initializing RainMaker API...");
        char *refresh_token = NULL;
        if (esp_rmaker_auth_service_get_user_token(&refresh_token) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get user token");
            return ESP_FAIL;
        }
        char *base_url = NULL;
        if (esp_rmaker_auth_service_get_base_url(&base_url) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get base URL");
            if (refresh_token) {
                free(refresh_token);
            }
            return ESP_FAIL;
        }
        esp_err_t ret = rainmaker_cli_handler_init(refresh_token, base_url);
        if (refresh_token) {
            free(refresh_token);
        }
        if (base_url) {
            free(base_url);
        }
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize RainMaker API");
            return ESP_FAIL;
        }
        is_rainmaker_api_initialized = true;
    }
    if (strcmp(argv[0], CMD_GET_NODES) == 0) {
        return get_nodes_command_handler(argc, argv);
    } else if (strcmp(argv[0], CMD_GET_NODE_DETAILS) == 0) {
        return get_node_details_command_handler(argc, argv);
    } else if (strcmp(argv[0], CMD_GET_SCHEDULES) == 0) {
        return get_schedules_command_handler(argc, argv);
    } else if (strcmp(argv[0], CMD_SET_SCHEDULE) == 0) {
        return set_schedules_command_handler(argc, argv);
    } else if (strcmp(argv[0], CMD_GET_NODE_CONFIG) == 0) {
        return get_node_config_command_handler(argc, argv);
    } else if (strcmp(argv[0], CMD_GET_NODE_STATUS) == 0) {
        return get_node_status_command_handler(argc, argv);
    } else if (strcmp(argv[0], CMD_SET_NODE_PARAMS) == 0) {
        return set_node_params_command_handler(argc, argv);
    } else if (strcmp(argv[0], CMD_GET_NODE_PARAMS) == 0) {
        return get_node_params_command_handler(argc, argv);
    } else if (strcmp(argv[0], CMD_REMOVE_NODE) == 0) {
        return remove_node_command_handler(argc, argv);
    }
    ESP_LOGW(TAG, "Unknown command: %s", argv[0]);
    return ESP_ERR_INVALID_ARG;
}

/* ---------- Command registration ---------- */

static void register_get_nodes_commands(void)
{
    const esp_console_cmd_t cmd = {
        .command = CMD_GET_NODES,
        .help = "List all nodes associated with the user",
        .hint = NULL,
        .func = &app_cli_command_handler,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_get_node_details_commands(void)
{
    const esp_console_cmd_t cmd = {
        .command = CMD_GET_NODE_DETAILS,
        .help = "Get detailed information for all nodes including config, status, and params",
        .hint = NULL,
        .func = &app_cli_command_handler,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_get_schedules_commands(void)
{
    const esp_console_cmd_t cmd = {
        .command = CMD_GET_SCHEDULES,
        .help = "Get schedule information for a specific node",
        .hint = NULL,
        .func = &app_cli_command_handler,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_set_schedule_commands(void)
{
    const esp_console_cmd_t cmd = {
        .command = CMD_SET_SCHEDULE,
        .help = "Manage schedules for a specific node",
        .hint = NULL,
        .func = &app_cli_command_handler,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_get_node_config_commands(void)
{
    const esp_console_cmd_t cmd = {
        .command = CMD_GET_NODE_CONFIG,
        .help = "Get node configuration",
        .hint = NULL,
        .func = &app_cli_command_handler,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_get_node_status_commands(void)
{
    const esp_console_cmd_t cmd = {
        .command = CMD_GET_NODE_STATUS,
        .help = "Get online/offline status of the node",
        .hint = NULL,
        .func = &app_cli_command_handler,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_set_node_params_commands(void)
{
    const esp_console_cmd_t cmd = {
        .command = CMD_SET_NODE_PARAMS,
        .help = "Set node parameters (use single quotes for JSON payload)",
        .hint = NULL,
        .func = &app_cli_command_handler,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_get_node_params_commands(void)
{
    const esp_console_cmd_t cmd = {
        .command = CMD_GET_NODE_PARAMS,
        .help = "Get node parameters",
        .hint = NULL,
        .func = &app_cli_command_handler,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_remove_node_commands(void)
{
    const esp_console_cmd_t cmd = {
        .command = CMD_REMOVE_NODE,
        .help = "Remove user-node mapping",
        .hint = NULL,
        .func = &app_cli_command_handler,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_get_heap_status_commands(void)
{
    const esp_console_cmd_t cmd = {
        .command = CMD_GET_HEAP_STATUS,
        .help = "Get heap memory status (free heap size and minimum free heap size)",
        .hint = NULL,
        .func = &get_heap_status_command_handler,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

void app_cli_command_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.task_stack_size = 8192;
    repl_config.prompt = PROMPT_STR;
    repl_config.max_cmdline_length = 1024;
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    esp_console_register_help_command();

    register_get_nodes_commands();
    register_get_node_details_commands();
    register_get_schedules_commands();
    register_set_schedule_commands();
    register_get_node_config_commands();
    register_get_node_status_commands();
    register_set_node_params_commands();
    register_get_node_params_commands();
    register_remove_node_commands();
    register_get_heap_status_commands();
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
