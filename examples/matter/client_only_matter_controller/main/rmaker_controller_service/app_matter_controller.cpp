/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_matter_controller_utils.h>
#include <esp_matter_core.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_utils.h>
#include <esp_timer.h>

#include <cstdio>
#include <stdint.h>
#include <string.h>

#include <lib/support/Span.h>

#include <app_matter_controller.h>
#include <app_matter_controller_creds_issuer.h>
#include <matter_controller_std.h>

#define TAG "MatterController"
#define MATTER_CTL_CMD_UPDATE_NOC 1
#define MATTER_CTL_CMD_UPDATE_DEVICE_LIST 2

static matter_controller_handle_t *s_matter_controller_handle = NULL;
static matter_controller_callback_t s_matter_controller_callback = NULL;

//static void debug_print()
//{
//    ESP_LOGI(TAG, "matter controller handle: {");
//    if (s_matter_controller_handle) {
//        if (s_matter_controller_handle->base_url) {
//            ESP_LOGI(TAG, "base_url: %s", s_matter_controller_handle->base_url);
//        }
//        if (s_matter_controller_handle->user_token) {
//            ESP_LOGI(TAG, "user_token: %s", s_matter_controller_handle->user_token);
//        }
//        if (s_matter_controller_handle->access_token) {
//            ESP_LOGI(TAG, "access_token: %s", s_matter_controller_handle->access_token);
//        }
//        if (s_matter_controller_handle->rmaker_group_id) {
//            ESP_LOGI(TAG, "rmaker_group_id: %s", s_matter_controller_handle->rmaker_group_id);
//        }
//        if (s_matter_controller_handle->matter_fabric_id > 0) {
//            ESP_LOGI(TAG, "matter_fabric_id: 0x%llX", s_matter_controller_handle->matter_fabric_id);
//        }
//        if (s_matter_controller_handle->matter_node_id > 0) {
//            ESP_LOGI(TAG, "matter_node_id: 0x%llX", s_matter_controller_handle->matter_node_id);
//        }
//        if (s_matter_controller_handle->matter_vendor_id > 0) {
//            ESP_LOGI(TAG, "matter_vendor_id: 0x%x", s_matter_controller_handle->matter_vendor_id);
//        }
//    }
//    ESP_LOGI(TAG, "}");
//}

static esp_err_t update_matter_fabric_id(uint64_t matter_fabric_id)
{
    s_matter_controller_handle->matter_fabric_id = matter_fabric_id;
    return ESP_OK;
}

static esp_err_t update_rmaker_group_id(const char *rmaker_group_id, const esp_rmaker_device_t *service,
                                        esp_rmaker_write_ctx_t *ctx)
{
    if (s_matter_controller_handle->rmaker_group_id) {
        free(s_matter_controller_handle->rmaker_group_id);
        s_matter_controller_handle->rmaker_group_id = NULL;
    }
    size_t size_to_copy = 0;
    if (rmaker_group_id && (size_to_copy = strlen(rmaker_group_id)) > 0) {
        s_matter_controller_handle->rmaker_group_id = (char *)MEM_CALLOC_EXTRAM(size_to_copy + 1, 1);
        if (!s_matter_controller_handle->rmaker_group_id) {
            return ESP_ERR_NO_MEM;
        }
        strncpy(s_matter_controller_handle->rmaker_group_id, rmaker_group_id, size_to_copy);
    }
    if (ctx->src != ESP_RMAKER_REQ_SRC_INIT) {
        esp_rmaker_param_t *param =
            esp_rmaker_device_get_param_by_type(service, ESP_RMAKER_PARAM_RMAKER_GROUP_ID);
        ESP_RETURN_ON_FALSE(param, ESP_ERR_NOT_FOUND, TAG, "Cannot find the rmaker group id param");
        esp_rmaker_param_val_t val =
            esp_rmaker_str(s_matter_controller_handle->rmaker_group_id ? s_matter_controller_handle->rmaker_group_id : "");
        ESP_RETURN_ON_ERROR(esp_rmaker_param_update_and_report(param, val), TAG, "Failed to update and report rmaker group id");
    }
    // Matter fabric ID will be invalid after updating RainMaker Group ID
    return update_matter_fabric_id(0);
}

static esp_err_t update_access_token(const char *access_token)
{
    if (s_matter_controller_handle->access_token) {
        free(s_matter_controller_handle->access_token);
        s_matter_controller_handle->access_token = NULL;
    }
    size_t size_to_copy = 0;
    if (access_token && (size_to_copy = strlen(access_token)) > 0) {
        s_matter_controller_handle->access_token = (char *)MEM_CALLOC_EXTRAM(size_to_copy + 1, 1);
        if (!s_matter_controller_handle->access_token) {
            return ESP_ERR_NO_MEM;
        }
        strncpy(s_matter_controller_handle->access_token, access_token, size_to_copy);
    }
    return ESP_OK;
}

static esp_err_t update_user_token(const char *user_token, const esp_rmaker_device_t *service,
                                   esp_rmaker_write_ctx_t *ctx)
{
    if (s_matter_controller_handle->user_token) {
        free(s_matter_controller_handle->user_token);
        s_matter_controller_handle->user_token = NULL;
    }
    size_t size_to_copy = 0;
    if (user_token && (size_to_copy = strlen(user_token)) > 0) {
        s_matter_controller_handle->user_token = (char *)MEM_CALLOC_EXTRAM(size_to_copy + 1, 1);
        if (!s_matter_controller_handle->user_token) {
            return ESP_ERR_NO_MEM;
        }
        strncpy(s_matter_controller_handle->user_token, user_token, size_to_copy);
    }
    if (ctx->src != ESP_RMAKER_REQ_SRC_INIT) {
        esp_rmaker_param_t *param =
            esp_rmaker_device_get_param_by_type(service, ESP_RMAKER_PARAM_USER_TOKEN);
        ESP_RETURN_ON_FALSE(param, ESP_ERR_NOT_FOUND, TAG, "Cannot find the user token param");
        esp_rmaker_param_val_t val =
            esp_rmaker_str(s_matter_controller_handle->user_token ? s_matter_controller_handle->user_token : "");
        ESP_RETURN_ON_ERROR(esp_rmaker_param_update_and_report(param, val), TAG, "Failed to update and report user token");
    }
    // Access token will be invalid after updating user token
    return update_access_token(NULL);
}

static esp_err_t update_base_url(const char *base_url, const esp_rmaker_device_t *service,
                                 esp_rmaker_write_ctx_t *ctx)
{
    if (s_matter_controller_handle->base_url) {
        free(s_matter_controller_handle->base_url);
        s_matter_controller_handle->base_url = NULL;
    }
    size_t size_to_copy = 0;
    if (base_url && (size_to_copy = strlen(base_url)) > 0) {
        s_matter_controller_handle->base_url = (char *)MEM_CALLOC_EXTRAM(size_to_copy + 1, 1);
        if (!s_matter_controller_handle->base_url) {
            return ESP_ERR_NO_MEM;
        }
        strncpy(s_matter_controller_handle->base_url, base_url, size_to_copy);
    }
    if (ctx->src != ESP_RMAKER_REQ_SRC_INIT) {
        esp_rmaker_param_t *param =
            esp_rmaker_device_get_param_by_type(service, ESP_RMAKER_PARAM_BASE_URL);
        ESP_RETURN_ON_FALSE(param, ESP_ERR_NOT_FOUND, TAG, "Cannot find the base URL param");
        esp_rmaker_param_val_t val =
            esp_rmaker_str(s_matter_controller_handle->base_url ? s_matter_controller_handle->base_url : "");
        ESP_RETURN_ON_ERROR(esp_rmaker_param_update_and_report(param, val), TAG, "Failed to update and report base URL");
    }
    // User token and rmaker group id will be invalid after updating base URL
    ESP_RETURN_ON_ERROR(update_user_token(NULL, service, ctx), TAG, "Failed to update user token");
    return update_rmaker_group_id(NULL, service, ctx);
}

static void refresh_access_token(void *args)
{
    if (update_access_token(NULL) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset access_token to NULL");
        return;
    }
    if (matter_controller_handle_update() != ESP_OK) {
        ESP_LOGE(TAG, "Failed on matter_controller_handle_update");
    }
}

static esp_err_t report_matter_node_id(uint64_t matter_node_id)
{
    char matter_node_id_str[17];
    sprintf(matter_node_id_str, "%016llX", matter_node_id);
    esp_rmaker_param_val_t val;
    val.type = RMAKER_VAL_TYPE_STRING;
    val.val.s = matter_node_id_str;
    esp_rmaker_param_t *param =
        esp_rmaker_device_get_param_by_type(s_matter_controller_handle->service, ESP_RMAKER_PARAM_MATTER_NODE_ID);
    return esp_rmaker_param_update_and_report(param, val);
}

esp_err_t matter_controller_handle_update()
{
    matter_controller_handle_t *handle = s_matter_controller_handle;
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->base_url && handle->user_token && !handle->access_token) {
        // Do authorizing
        static esp_timer_handle_t access_token_refresh_timer = NULL;
        esp_err_t err = ESP_OK;
        if ((err = s_matter_controller_callback(handle, MATTER_CONTROLLER_CALLBACK_TYPE_AUTHORIZE)) != ESP_OK) {
            ESP_LOGE(TAG, "Failed on authorizing");
            // The above function will failed with ESP_ERR_INVALID_RESPONSE if the user_token is invalid (This can happen
            // when the user changes the password). In this case, we need to update_user_token to an empty string to notify
            // the phone APP so that it can update user_token with the new valid refresh_token.
            if (err == ESP_ERR_INVALID_RESPONSE) {
                esp_rmaker_write_ctx_t write_ctx;
                write_ctx.src = ESP_RMAKER_REQ_SRC_LOCAL;
                update_user_token(nullptr, handle->service, &write_ctx);
            }
            return err;
        }
        if (!access_token_refresh_timer) {
            esp_err_t err = ESP_OK;
            const esp_timer_create_args_t access_token_refresh_timer_args = {
                .callback = refresh_access_token,
            };
            ESP_RETURN_ON_ERROR(esp_timer_create(&access_token_refresh_timer_args, &access_token_refresh_timer), TAG,
                                "Failed to create access_token refresh timer");
            err = esp_timer_start_periodic(access_token_refresh_timer, (uint64_t)58 * 60 * 1000 * 1000 /* 58 mins */);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start access_token_refresh_timer");
                esp_timer_delete(access_token_refresh_timer);
                access_token_refresh_timer = NULL;
                return err;
            }
        }
    }
    if (handle->base_url && handle->access_token && handle->rmaker_group_id && handle->matter_fabric_id == 0) {
        // Fetch Matter Fabric ID
        ESP_RETURN_ON_ERROR(
            s_matter_controller_callback(handle, MATTER_CONTROLLER_CALLBACK_TYPE_QUERY_MATTER_FABRIC_ID),
            TAG, "Failed on fetching Matter Fabric ID");
    }
    if (handle->base_url && handle->access_token && handle->rmaker_group_id && handle->matter_fabric_id != 0 &&
        handle->matter_node_id == 0) {
        // Setup Matter Controller
        ESP_RETURN_ON_ERROR(
            s_matter_controller_callback(handle, MATTER_CONTROLLER_CALLBACK_TYPE_SETUP_CONTROLLER), TAG,
            "Failed on setting up controller");
    }
    if (handle->base_url && handle->access_token && handle->rmaker_group_id && handle->matter_fabric_id != 0 &&
        handle->matter_node_id != 0) {
        report_matter_node_id(handle->matter_node_id);
    }
    matter_controller_status_t status;
    status.base_url_set = handle->base_url ? 1 : 0;
    status.user_token_set = handle->user_token ? 1: 0;
    status.access_token_set = handle->access_token ? 1 : 0;
    status.rmaker_group_id_set = handle->rmaker_group_id ? 1 : 0;
    status.matter_node_id_set = handle->matter_node_id != 0;
    status.matter_fabric_id_set = handle->matter_fabric_id != 0;
    status.matter_noc_installed = handle->matter_noc_installed;
    matter_controller_report_status(status);
    return ESP_OK;
}

static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                          const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (!s_matter_controller_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(esp_rmaker_param_get_type(param), ESP_RMAKER_PARAM_BASE_URL) == 0) {
        if (val.type != RMAKER_VAL_TYPE_STRING || !val.val.s) {
            return ESP_ERR_INVALID_ARG;
        }
        ESP_RETURN_ON_ERROR(update_base_url(val.val.s, device, ctx), TAG, "Failed to update base_url");
    } else if (strcmp(esp_rmaker_param_get_type(param), ESP_RMAKER_PARAM_USER_TOKEN) == 0) {
        if (val.type != RMAKER_VAL_TYPE_STRING || !val.val.s) {
            return ESP_ERR_INVALID_ARG;
        }
        ESP_RETURN_ON_ERROR(update_user_token(val.val.s, device, ctx), TAG, "Failed to update user_token");
    } else if (strcmp(esp_rmaker_param_get_type(param), ESP_RMAKER_PARAM_RMAKER_GROUP_ID) == 0) {
        if (val.type != RMAKER_VAL_TYPE_STRING || !val.val.s) {
            return ESP_ERR_INVALID_ARG;
        }
        ESP_RETURN_ON_ERROR(update_rmaker_group_id(val.val.s, device, ctx), TAG, "Failed to update rmaker_group_id");
    } else if (strcmp(esp_rmaker_param_get_type(param), ESP_RMAKER_PARAM_MATTER_CTL_CMD) == 0) {
        if (val.type != RMAKER_VAL_TYPE_INTEGER) {
            return ESP_ERR_INVALID_ARG;
        }
        if (val.val.i == MATTER_CTL_CMD_UPDATE_NOC) {
            if (!s_matter_controller_handle->base_url || !s_matter_controller_handle->user_token ||
                !s_matter_controller_handle->access_token || !s_matter_controller_handle->rmaker_group_id ||
                s_matter_controller_handle->matter_fabric_id == 0 || s_matter_controller_handle->matter_node_id == 0) {
                return ESP_ERR_INVALID_STATE;
            }
            ESP_RETURN_ON_ERROR(
                s_matter_controller_callback(s_matter_controller_handle, MATTER_CONTROLLER_CALLBACK_TYPE_UPDATE_CONTROLLER_NOC),
                TAG, "Failed on update controller NOC");
        } else if (val.val.i == MATTER_CTL_CMD_UPDATE_DEVICE_LIST) {
            ESP_RETURN_ON_ERROR(
                s_matter_controller_callback(s_matter_controller_handle, MATTER_CONTROLLER_CALLBACK_TYPE_UPDATE_DEVICE),
                TAG, "Failed on updating device list");
        }
    } else if (strcmp(esp_rmaker_param_get_type(param), ESP_RMAKER_PARAM_MATTER_CTL_STATUS) == 0) {
        if (val.type != RMAKER_VAL_TYPE_INTEGER) {
            return ESP_ERR_INVALID_ARG;
        }
        matter_controller_status_t status;
        status.raw = val.val.i;
        if (ctx->src == ESP_RMAKER_REQ_SRC_INIT) {
            // After reboot, we should know whether the NOC chain was installed before.
            s_matter_controller_handle->matter_noc_installed = status.matter_noc_installed;
        }
    }

    if (ctx->src != ESP_RMAKER_REQ_SRC_INIT) {
        // The updating function might need internet connection, so we do not call it when initializing.
        return matter_controller_handle_update();
    }
    return ESP_OK;
}

esp_err_t matter_controller_enable(uint16_t matter_vendor_id, matter_controller_callback_t callback)
{
    if (!callback) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_matter_controller_handle) {
        return ESP_OK;
    }
    s_matter_controller_handle = (matter_controller_handle_t *)MEM_CALLOC_EXTRAM(1, sizeof(matter_controller_handle_t));
    if (!s_matter_controller_handle) {
        ESP_LOGE(TAG, "Couldn't allocate s_matter_controller_handle");
        return ESP_ERR_NO_MEM;
    }
    s_matter_controller_handle->matter_vendor_id = matter_vendor_id;
    s_matter_controller_callback = callback;
    s_matter_controller_handle->service = matter_controller_service_create("MatterCTL", write_cb, NULL, NULL);
    if (!s_matter_controller_handle->service) {
        ESP_LOGE(TAG, "Failed to create MatterController Service");
        free(s_matter_controller_handle);
        s_matter_controller_handle = NULL;
        return ESP_FAIL;
    }
    esp_err_t err = esp_rmaker_node_add_device(esp_rmaker_get_node(), s_matter_controller_handle->service);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add service MatterController");
        free(s_matter_controller_handle);
        s_matter_controller_handle = NULL;
        return err;
    }

    static example_op_creds_issuer s_matter_controller_creds_issuer(s_matter_controller_handle);
    esp_matter::controller::set_custom_credentials_issuer(&s_matter_controller_creds_issuer);
    return ESP_OK;
}

esp_err_t matter_controller_report_status(matter_controller_status_t status)
{
    esp_rmaker_param_val_t val;
    val.type = RMAKER_VAL_TYPE_INTEGER;
    val.val.i = status.raw;
    esp_rmaker_param_t *param =
        esp_rmaker_device_get_param_by_type(s_matter_controller_handle->service, ESP_RMAKER_PARAM_MATTER_CTL_STATUS);
    return esp_rmaker_param_update_and_report(param, val);
}
