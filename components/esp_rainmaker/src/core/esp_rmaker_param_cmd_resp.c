/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_cmd_resp.h>
#include <esp_rmaker_utils.h>
#include <json_parser.h>
#include <json_generator.h>
#include "esp_rmaker_internal.h"

#define CMD_RESP_BUFFER_SIZE           CONFIG_ESP_RMAKER_PARAM_CMD_RESP_BUFFER_SIZE

/* Dynamic buffer allocated during initialization */
static char *s_cmd_response_buffer = NULL;

static const char *TAG = "esp_rmaker_param_cmd_resp";

/* Helper function to get JSON for changed parameters only */
static char *esp_rmaker_get_changed_params(void)
{
    if (s_cmd_response_buffer == NULL) {
        ESP_LOGE(TAG, "Command response buffer not allocated");
        return NULL;
    }

    size_t req_size = 0;
    /* Get size for changed parameters only */
    esp_err_t err = esp_rmaker_populate_params(NULL, &req_size, RMAKER_PARAM_FLAG_VALUE_CHANGE, false);
    if (err != ESP_OK || req_size < RMAKER_MIN_VALID_PARAMS_SIZE) {
        return NULL; /* No changed parameters */
    }

    req_size += RMAKER_PARAMS_SIZE_MARGIN;
    char *changed_params = MEM_CALLOC_EXTRAM(1, req_size);
    if (!changed_params) {
        return NULL;
    }

    err = esp_rmaker_populate_params(changed_params, &req_size, RMAKER_PARAM_FLAG_VALUE_CHANGE, true);
    if (err != ESP_OK) {
        free(changed_params);
        return NULL;
    }

    return changed_params;
}

/* Command handler for ESP_RMAKER_CMD_TYPE_SET_PARAMS (id=1)
 * This handler routes set params commands through the same path as regular param updates
 */
static esp_err_t esp_rmaker_set_params_cmd_handler(const void *in_data, size_t in_len,
                                                   void **out_data, size_t *out_len,
                                                   esp_rmaker_cmd_ctx_t *ctx, void *priv)
{
    if (in_data == NULL || in_len == 0) {
        ESP_LOGE(TAG, "No data received for set params command");
        return ESP_FAIL;
    }

    if (s_cmd_response_buffer == NULL) {
        ESP_LOGE(TAG, "Command response buffer not allocated");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Received set params command: %.*s", (int)in_len, (char *)in_data);

    /* Parse the input JSON to extract "params" object */
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, (char *)in_data, in_len) != 0) {
        ESP_LOGE(TAG, "Failed to parse input JSON");
        return ESP_FAIL;
    }

    /* Check if "params" object exists and get its size */
    int params_len = 0;
    if (json_obj_get_object_strlen(&jctx, "params", &params_len) != 0) {
        ESP_LOGE(TAG, "No 'params' object found in input or failed to get length");
        json_parse_end(&jctx);
        return ESP_FAIL;
    }

    params_len++; /* For null termination */

    /* Check if params will fit in our static buffer */
    if (params_len > CMD_RESP_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Params string too large for buffer (%d > %d)", params_len, CMD_RESP_BUFFER_SIZE);
        json_parse_end(&jctx);
        return ESP_ERR_NO_MEM;
    }

    /* Reuse static buffer for params extraction */
    memset(s_cmd_response_buffer, 0, CMD_RESP_BUFFER_SIZE);
    char *params_str = s_cmd_response_buffer;
    if (json_obj_get_object_str(&jctx, "params", params_str, params_len) != 0) {
        ESP_LOGE(TAG, "Failed to extract params object");
        json_parse_end(&jctx);
        return ESP_FAIL;
    }

    json_parse_end(&jctx);

    ESP_LOGI(TAG, "Extracted params: %s", params_str);

    /* Process the parameters - this will set RMAKER_PARAM_FLAG_VALUE_CHANGE flags */
    esp_err_t err = esp_rmaker_handle_set_params(params_str, strlen(params_str), ESP_RMAKER_REQ_SRC_CMD_RESP);

    /* Get JSON for only the changed parameters */
    char *changed_params_json = esp_rmaker_get_changed_params();

    /* Build the response JSON using static buffer */
    memset(s_cmd_response_buffer, 0, CMD_RESP_BUFFER_SIZE);
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, s_cmd_response_buffer, CMD_RESP_BUFFER_SIZE, NULL, NULL);
    json_gen_start_object(&jstr);

    /* Set status based on operation result */
    if (err == ESP_OK) {
        json_gen_obj_set_string(&jstr, "status", "success");
    } else {
        json_gen_obj_set_string(&jstr, "status", "fail");
    }

    /* Add changed parameters regardless of overall success/fail status */
    if (changed_params_json && strlen(changed_params_json) >= RMAKER_MIN_VALID_PARAMS_SIZE) {
        json_gen_push_object_str(&jstr, "params", changed_params_json);
    }

    json_gen_end_object(&jstr);
    size_t response_len = json_gen_str_end(&jstr);

    /* Cleanup - params_str is now pointing to static buffer, so no need to free it */
    if (changed_params_json) {
        free(changed_params_json);
    }

    /* Set output to static buffer */
    *out_data = s_cmd_response_buffer;
    *out_len = response_len - 1; /* Subtracting one as we need not send the null terminator */

    ESP_LOGI(TAG, "Returning response: %s", s_cmd_response_buffer);
    return ESP_OK;
}

/* Register the standard set params command handler (command id = 1)
 * This provides a unified API to enable parameter command-response functionality
 */
esp_err_t esp_rmaker_param_cmd_resp_enable(void)
{
    ESP_LOGI(TAG, "Enabling parameter command-response functionality");

    /* Allocate buffer for command-response operations */
    if (s_cmd_response_buffer == NULL) {
        s_cmd_response_buffer = MEM_CALLOC_EXTRAM(1, CMD_RESP_BUFFER_SIZE);
        if (s_cmd_response_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate command response buffer");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "Allocated %d bytes for command response buffer", CMD_RESP_BUFFER_SIZE);
    }

    esp_err_t err = esp_rmaker_cmd_register(ESP_RMAKER_CMD_TYPE_SET_PARAMS,
                                            ESP_RMAKER_USER_ROLE_SUPER_ADMIN |
                                            ESP_RMAKER_USER_ROLE_PRIMARY_USER |
                                            ESP_RMAKER_USER_ROLE_SECONDARY_USER,
                                            esp_rmaker_set_params_cmd_handler,
                                            false, /* free_on_return - using allocated buffer */
                                            NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register set params command handler");
        /* Cleanup allocated buffer on failure */
        if (s_cmd_response_buffer) {
            free(s_cmd_response_buffer);
            s_cmd_response_buffer = NULL;
        }
        return err;
    }

    ESP_LOGI(TAG, "Registered standard set params command handler (id=%d)", ESP_RMAKER_CMD_TYPE_SET_PARAMS);
    return ESP_OK;
}
