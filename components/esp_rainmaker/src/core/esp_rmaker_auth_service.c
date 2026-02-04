/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_err.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_auth_service.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_services.h>

ESP_EVENT_DEFINE_BASE(RMAKER_AUTH_SERVICE_EVENT);
#define ESP_RMAKER_AUTH_SERVICE_NAME           "RMUserAuth"

static const char *TAG = "esp_rmaker_auth_service";
static esp_rmaker_device_t *auth_service = NULL;

static bool esp_rmaker_auth_service_is_enabled(void)
{
    /* Return true if the auth service is enabled, otherwise return false */
    return (auth_service != NULL);
}

static char *esp_rmaker_auth_service_get_value_by_name(const char *name)
{
    esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_name(auth_service, name);
    if (!param) {
        ESP_LOGE(TAG, "Failed to get value by name: %s", name);
        return NULL;
    }
    esp_rmaker_param_val_t *val = esp_rmaker_param_get_val(param);
    if (!val) {
        ESP_LOGE(TAG, "Failed to get value by name: %s", name);
        return NULL;
    }
    if (val->type != RMAKER_VAL_TYPE_STRING) {
        ESP_LOGE(TAG, "Value by name: %s is not a string", name);
        return NULL;
    }
    if (val->val.s && strlen(val->val.s) > 0) {
        return val->val.s;
    }
    return NULL;
}

static esp_err_t esp_rmaker_auth_service_update_token_status_internal(esp_rmaker_user_auth_service_token_status_t status, bool need_report)
{
    if (!esp_rmaker_auth_service_is_enabled()) {
        ESP_LOGW(TAG, "Auth Service not enabled");
        return ESP_ERR_NOT_FOUND;
    }
    if (status >= ESP_RMAKER_USER_AUTH_SERVICE_TOKEN_STATUS_MAX) {
        ESP_LOGE(TAG, "Invalid user token status");
        return ESP_ERR_INVALID_ARG;
    }
    esp_rmaker_param_t *token_status_param = esp_rmaker_device_get_param_by_type(auth_service, ESP_RMAKER_PARAM_USER_TOKEN_STATUS);
    if (!token_status_param) {
        ESP_LOGE(TAG, "User Auth Service token status parameter not found");
        return ESP_ERR_NOT_FOUND;
    }
    /* Only update the parameter value if the status is changed */
    esp_rmaker_param_val_t *val = esp_rmaker_param_get_val(token_status_param);
    if (!val) {
        ESP_LOGE(TAG, "Failed to get user token status value");
        return ESP_ERR_NOT_FOUND;
    }
    if (val->val.i == (int)status) {
        return ESP_OK;
    }
    if (need_report) {
        esp_rmaker_param_update_and_report(token_status_param, esp_rmaker_int((int)status));
    } else {
        esp_rmaker_param_update(token_status_param, esp_rmaker_int((int)status));
    }
    return ESP_OK;
}

static esp_err_t esp_rmaker_auth_service_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_write_req_t write_req[], uint8_t count, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    esp_err_t err = ESP_OK;
    if (ctx && (ctx->src == ESP_RMAKER_REQ_SRC_INIT)) {
        /* If the write request is via INIT, do nothing */
        return ESP_OK;
    }
    ESP_LOGD(TAG, "Auth Service received %d params in write", count);

    for (int i = 0; i < count; i++) {
        const esp_rmaker_param_t *param = write_req[i].param;
        esp_rmaker_param_val_t val = write_req[i].val;
        const char *param_name = esp_rmaker_param_get_name(param);
        if (strcmp(param_name, ESP_RMAKER_DEF_USER_TOKEN_NAME) == 0) {
            ESP_LOGD(TAG, "Received value = %s for %s", (char *)val.val.s, param_name);
            /* Only update the token status if the token is set and no need to report here */
            err = esp_rmaker_auth_service_update_token_status_internal(ESP_RMAKER_USER_AUTH_SERVICE_TOKEN_STATUS_NOT_VERIFIED, false);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to update user token status: %d", err);
            }
            /* Update the parameter value before posting the event */
            err = esp_rmaker_param_update(param, val);
            if (err == ESP_OK) {
                esp_event_post(RMAKER_AUTH_SERVICE_EVENT, RMAKER_AUTH_SERVICE_EVENT_TOKEN_RECEIVED, NULL, 0, portMAX_DELAY);
            } else {
                ESP_LOGE(TAG, "Failed to update %s parameter: %d", param_name, err);
            }
        } else if (strcmp(param_name, ESP_RMAKER_DEF_BASE_URL_NAME) == 0) {
            ESP_LOGD(TAG, "Received value = %s for %s", (char *)val.val.s, param_name);
            /* Update the parameter value before posting the event */
            err = esp_rmaker_param_update(param, val);
            if (err == ESP_OK) {
                esp_event_post(RMAKER_AUTH_SERVICE_EVENT, RMAKER_AUTH_SERVICE_EVENT_BASE_URL_RECEIVED, NULL, 0, portMAX_DELAY);
            } else {
                ESP_LOGE(TAG, "Failed to update %s parameter: %d", param_name, err);
            }
        } else {
            ESP_LOGI(TAG, "Ignoring update for %s", param_name);
        }
    }
    return err;
}

esp_err_t esp_rmaker_auth_service_enable(void)
{
    if (esp_rmaker_auth_service_is_enabled()) {
        ESP_LOGW(TAG, "Auth Service already enabled");
        return ESP_OK;
    }
    auth_service = esp_rmaker_create_user_auth_service(ESP_RMAKER_AUTH_SERVICE_NAME, esp_rmaker_auth_service_cb, NULL, NULL);
    if (!auth_service) {
        ESP_LOGE(TAG, "Failed to create Auth Service");
        return ESP_FAIL;
    }
    esp_err_t err = esp_rmaker_node_add_device(esp_rmaker_get_node(), auth_service);
    if (err == ESP_OK) {
        /* Check whether the user token has been set and if set, update the token status to not verified */
        char *user_token = esp_rmaker_auth_service_get_value_by_name(ESP_RMAKER_DEF_USER_TOKEN_NAME);
        if (user_token) {
            err = esp_rmaker_auth_service_update_token_status_internal(ESP_RMAKER_USER_AUTH_SERVICE_TOKEN_STATUS_NOT_VERIFIED, false);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to update user token status: %d", err);
            }
        }
        ESP_LOGI(TAG, "Auth Service enabled");
        esp_event_post(RMAKER_AUTH_SERVICE_EVENT, RMAKER_AUTH_SERVICE_EVENT_ENABLED, NULL, 0, portMAX_DELAY);
    } else {
        esp_rmaker_device_delete(auth_service);
        auth_service = NULL;
    }
    return err;
}

esp_err_t esp_rmaker_auth_service_disable(void)
{
    if (!esp_rmaker_auth_service_is_enabled()) {
        ESP_LOGW(TAG, "Auth Service already disabled");
        return ESP_OK;
    }

    /* Remove the auth service from the node */
    esp_err_t err = esp_rmaker_node_remove_device(esp_rmaker_get_node(), auth_service);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to remove auth service from node");
        return err;
    }
    err = esp_rmaker_device_delete(auth_service);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete auth service");
        return err;
    }
    auth_service = NULL;
    ESP_LOGI(TAG, "Auth Service disabled");

    /* Report the node details */
    err = esp_rmaker_report_node_details();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to report node details");
        /* Regard as success if report fails */
        err = ESP_OK;
    }
    esp_event_post(RMAKER_AUTH_SERVICE_EVENT, RMAKER_AUTH_SERVICE_EVENT_DISABLED, NULL, 0, portMAX_DELAY);
    return err;
}

esp_err_t esp_rmaker_user_auth_service_token_status_update(esp_rmaker_user_auth_service_token_status_t status)
{
    return esp_rmaker_auth_service_update_token_status_internal(status, true);
}

esp_err_t esp_rmaker_auth_service_get_user_token(char **user_token)
{
    char *value = esp_rmaker_auth_service_get_value_by_name(ESP_RMAKER_DEF_USER_TOKEN_NAME);
    if (!value) {
        return ESP_ERR_NOT_FOUND;
    }
    *user_token = strdup(value);
    if (!*user_token) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t esp_rmaker_auth_service_get_base_url(char **base_url)
{
    char *value = esp_rmaker_auth_service_get_value_by_name(ESP_RMAKER_DEF_BASE_URL_NAME);
    if (!value) {
        return ESP_ERR_NOT_FOUND;
    }
    *base_url = strdup(value);
    if (!*base_url) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
