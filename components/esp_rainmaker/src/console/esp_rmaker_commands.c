/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) PTE LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_console.h>
#include <esp_partition.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_user_mapping.h>
#include <esp_rmaker_utils.h>
#include <esp_rmaker_cmd_resp.h>
#include <esp_rmaker_internal.h>
#include <network_provisioning/manager.h>
#include <sdkconfig.h>

/* Include internal header to access device structure */
#include "esp_rmaker_internal.h"

static const char *TAG = "esp_rmaker_commands";


static int user_node_mapping_handler(int argc, char** argv)
{
    if (argc == 3) {
        printf("%s: Starting user-node mapping\n", TAG);
        return esp_rmaker_start_user_node_mapping(argv[1], argv[2]);
    } else {
        printf("%s: Invalid Usage.\n", TAG);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static void register_user_node_mapping()
{
    const esp_console_cmd_t cmd = {
        .command = "add-user",
        .help = "Initiate the User-Node mapping from the node. Usage: add-user <user_id> <secret_key>",
        .func = &user_node_mapping_handler,
    };
    ESP_LOGI(TAG, "Registering command: %s", cmd.command);
    esp_console_cmd_register(&cmd);
}

static int get_node_id_handler(int argc, char** argv)
{
    printf("%s: Node ID: %s\n", TAG, esp_rmaker_get_node_id());
    return ESP_OK;
}

static void register_get_node_id()
{
    const esp_console_cmd_t cmd = {
        .command = "get-node-id",
        .help = "Get the Node ID for this board",
        .func = &get_node_id_handler,
    };
    ESP_LOGI(TAG, "Registering command: %s", cmd.command);
    esp_console_cmd_register(&cmd);
}

static int wifi_prov_handler(int argc, char** argv)
{
#ifdef CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI
    if (argc < 2) {
        printf("%s: Invalid Usage.\n", TAG);
        return ESP_ERR_INVALID_ARG;
    }
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    memcpy(wifi_config.sta.ssid, argv[1], strlen(argv[1]));
    if (argc == 3) {
        memcpy(wifi_config.sta.password, argv[2], strlen(argv[2]));
    }

    /* If device is still provisioning, use  network_prov_mgr_configure_wifi_sta/wifi_prov_mgr_configure_sta */
    bool provisioned = false;
    network_prov_mgr_is_wifi_provisioned(&provisioned);
    if (!provisioned) { // provisioning in progress
        network_prov_mgr_configure_wifi_sta(&wifi_config);
        return ESP_OK;
    }

    /* If already provisioned, just set the new credentials */
    /* Stop the Wi-Fi */
    if (esp_wifi_stop() != ESP_OK) {
        printf("%s: Failed to stop wifi\n", TAG);
    }
    /* Configure Wi-Fi station with provided host credentials */
    if (esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) != ESP_OK) {
        printf("%s: Failed to set WiFi configuration\n", TAG);
        return ESP_FAIL;
    }
    /* (Re)Start Wi-Fi */
    if (esp_wifi_start() != ESP_OK) {
        printf("%s: Failed to start WiFi\n", TAG);
        return ESP_FAIL;
    }
    /* Connect to AP */
    if (esp_wifi_connect() != ESP_OK) {
        printf("%s: Failed to connect WiFi\n", TAG);
        return ESP_FAIL;
    }
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif /* CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI */
}

static void register_wifi_prov()
{
    const esp_console_cmd_t cmd = {
        .command = "wifi-prov",
        .help = "Wi-Fi Provision the node. Usage: wifi-prov <ssid> [<passphrase>]",
        .func = &wifi_prov_handler,
    };
    ESP_LOGI(TAG, "Registering command: %s", cmd.command);
    esp_console_cmd_register(&cmd);
}
#ifdef CONFIG_ESP_RMAKER_CMD_RESP_ENABLE
static int cmd_resp_cli_handler(int argc, char *argv[])
{
    if (argc != 5) {
        printf("Usage: cmd <req_id> <user_role> <cmd> <data>\n");
        return -1;
    }
    char *req_id = argv[1];
    uint8_t user_role = atoi(argv[2]);
    uint16_t cmd = atoi(argv[3]);
    esp_rmaker_cmd_resp_test_send(req_id, user_role, cmd, (void *)argv[4], strlen(argv[4]), esp_rmaker_test_cmd_resp, NULL);
    return 0;
}

static void register_cmd_resp_command()
{
    const esp_console_cmd_t cmd_resp_cmd = {
        .command = "cmd",
        .help = "Send command to command-response module. Usage cmd <req_id> <cmd> <user_role> <data>",
        .func = &cmd_resp_cli_handler,
    };
    ESP_LOGI(TAG, "Registering command: %s", cmd_resp_cmd.command);
    esp_console_cmd_register(&cmd_resp_cmd);
}
#endif

static int sign_data_command(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: sign-data <data>\n");
        return -1;
    }
    char *data = (char *)argv[1];
    size_t outlen = 0;
    char *response = NULL;
    esp_err_t err = esp_rmaker_node_auth_sign_msg((const void *)data, strlen(data), (void **)&response, &outlen);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to sign message");
        return -1;
    }
    ESP_LOGI(TAG, "hex signature(len %d): %s", outlen, response);
    free(response);
    return 0;
}

static void register_sign_data_command()
{
    const esp_console_cmd_t cmd = {
        .command = "sign-data",
        .help = "sends some data and expects a rsa signed response",
        .func = &sign_data_command,
    };
    ESP_LOGI(TAG, "Registering command: %s", cmd.command);
    esp_console_cmd_register(&cmd);
}

#ifdef CONFIG_ESP_RMAKER_CONSOLE_PARAM_CMDS_ENABLE
static int set_param_handler(int argc, char** argv)
{
    if (argc != 4) {
        printf("%s: Invalid Usage.\n", TAG);
        printf("Usage: set-param <device_name> <param_name> <value>\n");
        printf("  Note: This command invokes device callbacks (simulates real parameter changes)\n");
        return ESP_ERR_INVALID_ARG;
    }

    const char *device_name = argv[1];
    const char *param_name = argv[2];
    const char *value_str_raw = argv[3];

    /* Strip surrounding quotes if present */
    char *value_str = strdup(value_str_raw);
    if (!value_str) {
        printf("%s: Failed to process value string\n", TAG);
        return ESP_ERR_NO_MEM;
    }

    // Get the device handle
    esp_rmaker_device_t *device = esp_rmaker_node_get_device_by_name(esp_rmaker_get_node(), device_name);
    if (!device) {
        printf("%s: Device %s not found\n", TAG, device_name);
        free(value_str);
        return ESP_FAIL;
    }

    // Get the parameter handle
    esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_name(device, param_name);
    if (!param) {
        printf("%s: Parameter %s not found in device %s\n", TAG, param_name, device_name);
        free(value_str);
        return ESP_FAIL;
    }

    // Get current value to determine type
    esp_rmaker_param_val_t *val = esp_rmaker_param_get_val(param);
    if (!val) {
        printf("%s: Failed to get parameter value\n", TAG);
        free(value_str);
        return ESP_FAIL;
    }


    // Update value based on type
    esp_rmaker_param_val_t new_val;
    new_val.type = val->type;

    switch(val->type) {
        case RMAKER_VAL_TYPE_BOOLEAN:
            new_val.val.b = (strcmp(value_str, "true") == 0 || strcmp(value_str, "1") == 0);
            break;
        case RMAKER_VAL_TYPE_INTEGER:
            new_val.val.i = atoi(value_str);
            break;
        case RMAKER_VAL_TYPE_FLOAT:
            new_val.val.f = atof(value_str);
            break;
        case RMAKER_VAL_TYPE_STRING:
            new_val.val.s = strdup(value_str);  // Create a copy to avoid lifetime issues
            if (!new_val.val.s) {
                printf("%s: Failed to allocate memory for string value\n", TAG);
                free(value_str);
                return ESP_FAIL;
            }
            break;
        case RMAKER_VAL_TYPE_OBJECT:
            new_val.val.s = strdup(value_str);  // JSON objects are stored as strings
            if (!new_val.val.s) {
                printf("%s: Failed to allocate memory for object value\n", TAG);
                free(value_str);
                return ESP_FAIL;
            }
            break;
        case RMAKER_VAL_TYPE_ARRAY:
            new_val.val.s = strdup(value_str);  // JSON arrays are stored as strings
            if (!new_val.val.s) {
                printf("%s: Failed to allocate memory for array value\n", TAG);
                free(value_str);
                return ESP_FAIL;
            }
            break;
        default:
            printf("%s: Unsupported value type\n", TAG);
            free(value_str);
            return ESP_FAIL;
    }

    esp_err_t err = ESP_OK;

    /* Create JSON for esp_rmaker_handle_set_params to invoke callbacks */
    char json_str[1024];  /* Increased buffer size for complex JSON */
    int json_len = 0;

    switch(val->type) {
            case RMAKER_VAL_TYPE_BOOLEAN:
                json_len = snprintf(json_str, sizeof(json_str), "{\"%s\":{\"%s\":%s}}",
                                  device_name, param_name, new_val.val.b ? "true" : "false");
                break;
            case RMAKER_VAL_TYPE_INTEGER:
                json_len = snprintf(json_str, sizeof(json_str), "{\"%s\":{\"%s\":%d}}",
                                  device_name, param_name, new_val.val.i);
                break;
            case RMAKER_VAL_TYPE_FLOAT:
                json_len = snprintf(json_str, sizeof(json_str), "{\"%s\":{\"%s\":%f}}",
                                  device_name, param_name, new_val.val.f);
                break;
            case RMAKER_VAL_TYPE_STRING:
                json_len = snprintf(json_str, sizeof(json_str), "{\"%s\":{\"%s\":\"%s\"}}",
                                  device_name, param_name, new_val.val.s);
                break;
            case RMAKER_VAL_TYPE_OBJECT:
                /* JSON objects are embedded directly without quotes */
                json_len = snprintf(json_str, sizeof(json_str), "{\"%s\":{\"%s\":%s}}",
                                  device_name, param_name, new_val.val.s);
                break;
            case RMAKER_VAL_TYPE_ARRAY:
                /* JSON arrays are embedded directly without quotes */
                json_len = snprintf(json_str, sizeof(json_str), "{\"%s\":{\"%s\":%s}}",
                                  device_name, param_name, new_val.val.s);
                break;
            default:
                printf("%s: Unsupported value type for callback\n", TAG);
                /* Fall back to direct update */
                err = esp_rmaker_param_update_and_report(param, new_val);
                break;
        }

    if (json_len > 0 && json_len < sizeof(json_str)) {
        /* Use the standard parameter handling function to invoke callbacks */
        err = esp_rmaker_handle_set_params(json_str, json_len, ESP_RMAKER_REQ_SRC_FIRMWARE);
        if (err == ESP_OK) {
            printf("%s: Successfully set %s.%s with callback\n", TAG, device_name, param_name);
        } else {
            printf("%s: Callback failed for %s.%s\n", TAG, device_name, param_name);
        }
    } else {
        printf("%s: Failed to create JSON for callback (len=%d, max=%zu)\n", TAG, json_len, sizeof(json_str));
        err = ESP_FAIL;
    }

    /* Free allocated string memory if it was a string, object, or array type */
    if ((val->type == RMAKER_VAL_TYPE_STRING || val->type == RMAKER_VAL_TYPE_OBJECT || val->type == RMAKER_VAL_TYPE_ARRAY) && new_val.val.s) {
        free(new_val.val.s);
    }

    if (err != ESP_OK) {
        printf("%s: Failed to update parameter value\n", TAG);
        free(value_str);
        return err;
    }

    printf("%s: Successfully set %s.%s to %s\n", TAG, device_name, param_name, value_str);

    /* Free the processed value string */
    free(value_str);
    return ESP_OK;
}

static int update_param_handler(int argc, char** argv)
{
    if (argc != 4) {
        printf("%s: Invalid Usage.\n", TAG);
        printf("Usage: update-param <device_name> <param_name> <value>\n");
        printf("  Note: This command only updates the value without invoking callbacks\n");
        return ESP_ERR_INVALID_ARG;
    }

    const char *device_name = argv[1];
    const char *param_name = argv[2];
    const char *value_str_raw = argv[3];

    /* Strip surrounding quotes if present */
    char *value_str = strdup(value_str_raw);
    if (!value_str) {
        printf("%s: Failed to process value string\n", TAG);
        return ESP_ERR_NO_MEM;
    }

    // Get the device handle
    esp_rmaker_device_t *device = esp_rmaker_node_get_device_by_name(esp_rmaker_get_node(), device_name);
    if (!device) {
        printf("%s: Device %s not found\n", TAG, device_name);
        free(value_str);
        return ESP_FAIL;
    }

    // Get the parameter handle
    esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_name(device, param_name);
    if (!param) {
        printf("%s: Parameter %s not found in device %s\n", TAG, param_name, device_name);
        free(value_str);
        return ESP_FAIL;
    }

    // Get current value to determine type
    esp_rmaker_param_val_t *val = esp_rmaker_param_get_val(param);
    if (!val) {
        printf("%s: Failed to get parameter value\n", TAG);
        free(value_str);
        return ESP_FAIL;
    }

    // Update value based on type
    esp_rmaker_param_val_t new_val;
    new_val.type = val->type;

    switch(val->type) {
        case RMAKER_VAL_TYPE_BOOLEAN:
            new_val.val.b = (strcmp(value_str, "true") == 0 || strcmp(value_str, "1") == 0);
            break;
        case RMAKER_VAL_TYPE_INTEGER:
            new_val.val.i = atoi(value_str);
            break;
        case RMAKER_VAL_TYPE_FLOAT:
            new_val.val.f = atof(value_str);
            break;
        case RMAKER_VAL_TYPE_STRING:
            new_val.val.s = strdup(value_str);  // Create a copy to avoid lifetime issues
            if (!new_val.val.s) {
                printf("%s: Failed to allocate memory for string value\n", TAG);
                free(value_str);
                return ESP_FAIL;
            }
            break;
        case RMAKER_VAL_TYPE_OBJECT:
            new_val.val.s = strdup(value_str);  // JSON objects are stored as strings
            if (!new_val.val.s) {
                printf("%s: Failed to allocate memory for object value\n", TAG);
                free(value_str);
                return ESP_FAIL;
            }
            break;
        case RMAKER_VAL_TYPE_ARRAY:
            new_val.val.s = strdup(value_str);  // JSON arrays are stored as strings
            if (!new_val.val.s) {
                printf("%s: Failed to allocate memory for array value\n", TAG);
                free(value_str);
                return ESP_FAIL;
            }
            break;
        default:
            printf("%s: Unsupported value type\n", TAG);
            free(value_str);
            return ESP_FAIL;
    }

    /* Update the parameter value without invoking callback */
    esp_err_t err = esp_rmaker_param_update_and_report(param, new_val);

    /* Free allocated string memory if it was a string, object, or array type */
    if ((val->type == RMAKER_VAL_TYPE_STRING || val->type == RMAKER_VAL_TYPE_OBJECT || val->type == RMAKER_VAL_TYPE_ARRAY) && new_val.val.s) {
        free(new_val.val.s);
    }

    if (err != ESP_OK) {
        printf("%s: Failed to update parameter value\n", TAG);
        free(value_str);
        return err;
    }

    printf("%s: Successfully updated %s.%s to %s (no callback)\n", TAG, device_name, param_name, value_str);

    /* Free the processed value string */
    free(value_str);
    return ESP_OK;
}

static int get_param_handler(int argc, char** argv)
{
    if (argc != 3) {
        printf("%s: Invalid Usage.\n", TAG);
        printf("Usage: get-param <device_name> <param_name>\n");
        return ESP_ERR_INVALID_ARG;
    }

    const char *device_name = argv[1];
    const char *param_name = argv[2];

    // Get the device handle
    esp_rmaker_device_t *device = esp_rmaker_node_get_device_by_name(esp_rmaker_get_node(), device_name);
    if (!device) {
        printf("%s: Device %s not found\n", TAG, device_name);
        return ESP_FAIL;
    }

    // Get the parameter handle
    esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_name(device, param_name);
    if (!param) {
        printf("%s: Parameter %s not found in device %s\n", TAG, param_name, device_name);
        return ESP_FAIL;
    }

    // Get the parameter value
    esp_rmaker_param_val_t *val = esp_rmaker_param_get_val(param);
    if (!val) {
        printf("%s: Failed to get parameter value\n", TAG);
        return ESP_FAIL;
    }

    // Print value based on type
    switch(val->type) {
        case RMAKER_VAL_TYPE_BOOLEAN:
            printf("%s: %s.%s = %s\n", TAG, device_name, param_name, val->val.b ? "true" : "false");
            break;
        case RMAKER_VAL_TYPE_INTEGER:
            printf("%s: %s.%s = %d\n", TAG, device_name, param_name, val->val.i);
            break;
        case RMAKER_VAL_TYPE_FLOAT:
            printf("%s: %s.%s = %f\n", TAG, device_name, param_name, val->val.f);
            break;
        case RMAKER_VAL_TYPE_STRING:
            printf("%s: %s.%s = %s\n", TAG, device_name, param_name, val->val.s);
            break;
        case RMAKER_VAL_TYPE_OBJECT:
            printf("%s: %s.%s = %s\n", TAG, device_name, param_name, val->val.s);
            break;
        case RMAKER_VAL_TYPE_ARRAY:
            printf("%s: %s.%s = %s\n", TAG, device_name, param_name, val->val.s);
            break;
        default:
            printf("%s: Unsupported value type\n", TAG);
            return ESP_FAIL;
    }

    return ESP_OK;
}

static void register_param_commands()
{
    const esp_console_cmd_t set_cmd = {
        .command = "set-param",
        .help = "Set device parameter value with callback. Usage: set-param <device_name> <param_name> <value>",
        .func = &set_param_handler,
    };
    ESP_LOGI(TAG, "Registering command: %s", set_cmd.command);
    esp_console_cmd_register(&set_cmd);

    const esp_console_cmd_t update_cmd = {
        .command = "update-param",
        .help = "Update device parameter value without callback. Usage: update-param <device_name> <param_name> <value>",
        .func = &update_param_handler,
    };
    ESP_LOGI(TAG, "Registering command: %s", update_cmd.command);
    esp_console_cmd_register(&update_cmd);

    const esp_console_cmd_t get_cmd = {
        .command = "get-param",
        .help = "Get device parameter value. Usage: get-param <device_name> <param_name>",
        .func = &get_param_handler,
    };
    ESP_LOGI(TAG, "Registering command: %s", get_cmd.command);
    esp_console_cmd_register(&get_cmd);
}

#endif /* CONFIG_ESP_RMAKER_CONSOLE_PARAM_CMDS_ENABLE */

static int clear_claim_data_handler(int argc, char** argv)
{
    const char *partition_name = "fctry";
#ifdef CONFIG_ESP_RMAKER_FACTORY_PARTITION_NAME
    partition_name = CONFIG_ESP_RMAKER_FACTORY_PARTITION_NAME;
#endif

    printf("%s: Erasing fctry partition (%s)...\n", TAG, partition_name);

    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                                ESP_PARTITION_SUBTYPE_DATA_NVS,
                                                                partition_name);
    if (!partition) {
        printf("%s: Failed to find partition '%s'\n", TAG, partition_name);
        return ESP_FAIL;
    }

    esp_err_t err = esp_partition_erase_range(partition, 0, partition->size);
    if (err != ESP_OK) {
        printf("%s: Failed to erase partition '%s'. Error: %d\n", TAG, partition_name, err);
        return err;
    }

    printf("%s: Successfully erased fctry partition (%s). Rebooting...\n", TAG, partition_name);
    esp_rmaker_reboot(2);
    return ESP_OK;
}

static void register_clear_claim_data()
{
    const esp_console_cmd_t cmd = {
        .command = "clear-claim-data",
        .help = "Erase the fctry NVS partition (clears claim data)",
        .func = &clear_claim_data_handler,
    };
    ESP_LOGI(TAG, "Registering command: %s", cmd.command);
    esp_console_cmd_register(&cmd);
}

void esp_rmaker_register_commands()
{
    register_user_node_mapping();
    register_get_node_id();
    register_wifi_prov();
#ifdef CONFIG_ESP_RMAKER_CMD_RESP_ENABLE
    register_cmd_resp_command();
#endif
    register_sign_data_command();
    register_clear_claim_data();
#ifdef CONFIG_ESP_RMAKER_CONSOLE_PARAM_CMDS_ENABLE
    register_param_commands();
#endif
}
