/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_rmaker_auth_service.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_utils.h>
#include <nvs_flash.h>
#include <stdint.h>
#include <string.h>

#include "app_rmaker_matter_controller.h"
#include "app_rmaker_matter_controller_api.h"
#include "app_rmaker_matter_controller_internal.h"
#include "app_rmaker_matter_controller_service.h"
#include "app_rmaker_user_api.h"

#define TAG "rmaker_matter_controller"

static matter_controller_handle_t *s_matter_controller_handle = NULL;
static esp_err_t invoke_internal_callback(matter_controller_handle_t *handle, matter_controller_event_type_t type);
static esp_err_t update_device_list();

static void safe_free(char **ptr)
{
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

static bool check_handle_state()
{
    return s_matter_controller_handle && s_matter_controller_handle->is_authorized &&
        s_matter_controller_handle->rmaker_group_id;
}

static esp_err_t send_event_to_matter_ctl_task(matter_controller_event_type_t event_type)
{
    if (!s_matter_controller_handle || !s_matter_controller_handle->event_task_queue) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xQueueSend(s_matter_controller_handle->event_task_queue, &event_type, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t matter_controller_authorize(matter_controller_handle_t *handle)
{
    if (handle->is_authorized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(app_rmaker_user_api_set_base_url(handle->base_url), TAG, "Failed to set base URL");
    ESP_RETURN_ON_ERROR(app_rmaker_user_api_set_refresh_token(handle->user_token), TAG, "Failed to set user token");
    ESP_RETURN_ON_ERROR(app_rmaker_user_api_login(), TAG, "Failed to login");
    handle->is_authorized = true;
    return ESP_OK;
}

static esp_err_t matter_controller_setup_controller(matter_controller_handle_t *handle)
{
    if (handle->is_controller_setup) {
        return ESP_OK;
    }
    esp_err_t err = ESP_OK;
    if (handle->setup_callback) {
        uint64_t fabric_id = 0ULL;
        ESP_RETURN_ON_ERROR(matter_controller_authorize(handle), TAG, "Failed to authorize");
        ESP_RETURN_ON_ERROR(app_rmaker_api_get_matter_fabric_id(handle->rmaker_group_id, &fabric_id), TAG,
                            "Failed to get fabric ID");
        if (!handle->is_setup_successfully_before) {
            uint8_t ipk_value[ESP_MATTER_IPK_LEN];
            size_t ipk_len = ESP_MATTER_IPK_LEN;
            ESP_RETURN_ON_ERROR(app_rmaker_api_get_fabric_ipk(handle->rmaker_group_id, ipk_value, ESP_MATTER_IPK_LEN),
                                TAG, "Failed on fetching IPK");
            err = handle->setup_callback(ipk_value, ipk_len, fabric_id);
        } else {
            ESP_LOGI(TAG, "Controller has been successfully set up before");
            err = handle->setup_callback(NULL, 0, fabric_id);
        }
    } else {
        ESP_LOGE(TAG, "Please register a setup callback before calling app_rmaker_matter_controller_enable");
        return ESP_ERR_INVALID_STATE;
    }
    if (err == ESP_OK) {
        handle->is_controller_setup = true;
        handle->is_setup_successfully_before = true;
    }
    return err;
}

static esp_err_t report_matter_controller_status(matter_controller_handle_t *handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_STATE;
    }
    matter_controller_status_t status;
    status.raw = 0;
    status.base_url_set = handle->base_url ? 1 : 0;
    status.user_token_set = handle->user_token ? 1 : 0;
    status.rmaker_group_id_set = handle->rmaker_group_id ? 1 : 0;
    status.is_setup_successfully_before = handle->is_setup_successfully_before;
    status.matter_case_permission = handle->is_server_instance;
    esp_rmaker_param_val_t val = esp_rmaker_int(status.raw);
    esp_rmaker_param_t *param =
        esp_rmaker_device_get_param_by_type(handle->service, ESP_RMAKER_PARAM_MATTER_CTL_STATUS);
    return esp_rmaker_param_update_and_report(param, val);
}

static esp_err_t matter_controller_update_noc(matter_controller_handle_t *handle)
{
    ESP_RETURN_ON_ERROR(matter_controller_authorize(handle), TAG, "Failed to authorize before updating NOC");
    uint64_t fabric_id = 0ULL;
    if (handle->is_server_instance) {
        ESP_RETURN_ON_ERROR(app_rmaker_api_get_matter_fabric_id(handle->rmaker_group_id, &fabric_id), TAG,
                            "Failed to get fabric ID");
    }
    if (handle->update_noc_callback) {
        return handle->update_noc_callback(fabric_id);
    }
    ESP_LOGE(TAG, "Please register a update NOC callback when calling app_rmaker_matter_controller_enable");
    return ESP_FAIL;
}

static esp_err_t matter_controller_update_handle(matter_controller_handle_t *handle)
{
    if (handle->base_url && handle->user_token) {
        esp_err_t err = ESP_OK;
        if (!handle->is_authorized) {
            // Authorize Matter controller
            err = invoke_internal_callback(handle, MATTER_CONTROLLER_EVENT_TYPE_AUTHORIZE);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Failed on authorizing: %s", esp_err_to_name(err));
            }
        }

        if (handle->rmaker_group_id && !handle->is_controller_setup && handle->is_authorized) {
            // Setup Matter Controller
            err = invoke_internal_callback(handle, MATTER_CONTROLLER_EVENT_TYPE_SETUP_CONTROLLER);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Failed on setting up controller: %s", esp_err_to_name(err));
            }
        }
    }
    return report_matter_controller_status(handle);
}

static esp_err_t invoke_internal_callback(matter_controller_handle_t *handle, matter_controller_event_type_t type)
{
    switch (type) {
    case MATTER_CONTROLLER_EVENT_TYPE_AUTHORIZE: {
        return matter_controller_authorize(handle);
    }
    case MATTER_CONTROLLER_EVENT_TYPE_SETUP_CONTROLLER: {
        return matter_controller_setup_controller(handle);
    }
    case MATTER_CONTROLLER_EVENT_TYPE_UPDATE_CONTROLLER_NOC: {
        return matter_controller_update_noc(handle);
    }
    case MATTER_CONTROLLER_EVENT_TYPE_UPDATE_DEVICE_LIST: {
        return update_device_list();
    }
    case MATTER_CONTROLLER_EVENT_TYPE_UPDATE_HANDLE: {
        return matter_controller_update_handle(handle);
    }
    default:
        break;
    }
    return ESP_OK;
}

static void event_task_handler(void *pvParameters)
{
    matter_controller_event_type_t event_type;
    while (1) {
        if (xQueueReceive(s_matter_controller_handle->event_task_queue, &event_type, portMAX_DELAY) == pdTRUE) {
            invoke_internal_callback(s_matter_controller_handle, event_type);
        }
    }
}

static esp_err_t update_rmaker_group_id(const char *rmaker_group_id, const esp_rmaker_device_t *service,
                                        esp_rmaker_write_ctx_t *ctx)
{
    safe_free(&s_matter_controller_handle->rmaker_group_id);

    size_t size_to_copy = 0;
    if (rmaker_group_id && (size_to_copy = strlen(rmaker_group_id)) > 0) {
        s_matter_controller_handle->rmaker_group_id = (char *)MEM_CALLOC_EXTRAM(size_to_copy + 1, 1);
        if (!s_matter_controller_handle->rmaker_group_id) {
            return ESP_ERR_NO_MEM;
        }
        strncpy(s_matter_controller_handle->rmaker_group_id, rmaker_group_id, size_to_copy);
        s_matter_controller_handle->rmaker_group_id[size_to_copy] = '\0';
    }
    if (ctx->src != ESP_RMAKER_REQ_SRC_INIT) {
        esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_type(service, ESP_RMAKER_PARAM_RMAKER_GROUP_ID);
        ESP_RETURN_ON_FALSE(param, ESP_ERR_NOT_FOUND, TAG, "Cannot find the rmaker group id param");
        esp_rmaker_param_val_t val = esp_rmaker_str(
            s_matter_controller_handle->rmaker_group_id ? s_matter_controller_handle->rmaker_group_id : "");
        ESP_RETURN_ON_ERROR(esp_rmaker_param_update_and_report(param, val), TAG,
                            "Failed to update and report rmaker group id");
    }
    return ESP_OK;
}

static esp_err_t bulk_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_write_req_t write_req[],
                               uint8_t count, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (!s_matter_controller_handle) {
        ESP_LOGE(TAG, "Not initialized, call app_rmaker_matter_controller_enable first");
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < count; i++) {
        const esp_rmaker_param_t *param = write_req[i].param;
        esp_rmaker_param_val_t val = write_req[i].val;
        const char *param_type = esp_rmaker_param_get_type(param);
        if (!param_type) {
            return ESP_ERR_INVALID_ARG;
        }
        if (strcmp(param_type, ESP_RMAKER_PARAM_RMAKER_GROUP_ID) == 0) {
            if (val.type != RMAKER_VAL_TYPE_STRING || !val.val.s) {
                return ESP_ERR_INVALID_ARG;
            }
            ESP_RETURN_ON_ERROR(update_rmaker_group_id(val.val.s, device, ctx), TAG,
                                "Failed to update rmaker_group_id");
        } else if (strcmp(param_type, ESP_RMAKER_PARAM_MATTER_CTL_CMD) == 0) {
            if (val.type != RMAKER_VAL_TYPE_INTEGER) {
                return ESP_ERR_INVALID_ARG;
            }
            if (val.val.i == MATTER_CTL_CMD_UPDATE_NOC) {
                ESP_RETURN_ON_ERROR(send_event_to_matter_ctl_task(MATTER_CONTROLLER_EVENT_TYPE_UPDATE_CONTROLLER_NOC),
                                    TAG, "Failed to send update NOC request to task queue");
            } else if (val.val.i == MATTER_CTL_CMD_UPDATE_DEVICE_LIST) {
                ESP_RETURN_ON_ERROR(send_event_to_matter_ctl_task(MATTER_CONTROLLER_EVENT_TYPE_UPDATE_DEVICE_LIST), TAG,
                                    "Failed to send update device list request to task queue");
            }
        } else if (strcmp(param_type, ESP_RMAKER_PARAM_MATTER_CTL_STATUS) == 0) {
            if (val.type != RMAKER_VAL_TYPE_INTEGER) {
                return ESP_ERR_INVALID_ARG;
            }
            matter_controller_status_t status;
            status.raw = val.val.i;
            if (ctx->src == ESP_RMAKER_REQ_SRC_INIT) {
                // After reboot, we should know whether the controller has been successfully set up before.
                s_matter_controller_handle->is_setup_successfully_before = status.is_setup_successfully_before;
            }
        }
    }

    if (ctx->src != ESP_RMAKER_REQ_SRC_INIT) {
        // If not initializing, update the controller handle and report the status.
        return app_rmaker_matter_controller_handle_update();
    }
    return ESP_OK;
}

esp_err_t app_rmaker_matter_controller_issue_controller_noc(const uint8_t *csr_der, size_t csr_der_len,
                                                            uint8_t *noc_der, size_t *noc_der_len, uint64_t node_id,
                                                            uint8_t *serialized_keypair, size_t serialized_keypair_len)
{
    if (!check_handle_state()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_matter_controller_handle->is_server_instance || node_id == 0ULL) {
        ESP_RETURN_ON_ERROR(app_rmaker_api_create_matter_controller(esp_rmaker_get_node_id(),
                                                                    s_matter_controller_handle->rmaker_group_id,
                                                                    &s_matter_controller_handle->matter_node_id),
                            TAG, "Failed to create Matter Controller");
    } else {
        s_matter_controller_handle->matter_node_id = node_id;
    }

    ESP_RETURN_ON_ERROR(app_rmaker_api_issue_noc(csr_der, csr_der_len, s_matter_controller_handle->rmaker_group_id,
                                                 &s_matter_controller_handle->matter_node_id, noc_der, noc_der_len),
                        TAG, "Failed to issue NOC");

    if (!s_matter_controller_handle->is_server_instance &&
        (rmaker_matter_controller_set_nvs(MATTER_CTL_NVS_KEY_NOC, noc_der, *noc_der_len) != ESP_OK ||
         rmaker_matter_controller_set_nvs(MATTER_CTL_NVS_KEY_KEYPAIR, serialized_keypair, serialized_keypair_len) !=
             ESP_OK)) {
        // Still return ESP_OK if failed to store NOC or keypair
        ESP_LOGW(TAG, "Failed to store NOC or keypair");
    }
    return ESP_OK;
}

esp_err_t app_rmaker_matter_controller_get_stored_keypair_and_controller_noc(uint8_t *noc_der, size_t *noc_der_len,
                                                                             uint8_t *serialized_keypair,
                                                                             size_t *serialized_keypair_len)
{
    if (!noc_der || !noc_der_len || !serialized_keypair || !serialized_keypair_len) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = ESP_OK;
    err = rmaker_matter_controller_get_nvs(MATTER_CTL_NVS_KEY_NOC, noc_der, noc_der_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get stored NOC, maybe not installed yet");
        return err;
    }
    err = rmaker_matter_controller_get_nvs(MATTER_CTL_NVS_KEY_KEYPAIR, serialized_keypair, serialized_keypair_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get stored keypair, maybe not installed yet");
    }
    return err;
}

esp_err_t app_rmaker_matter_controller_fetch_rcac(uint8_t *rcac_der, size_t *rcac_der_len)
{
    if (!check_handle_state()) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_RETURN_ON_ERROR(
        app_rmaker_api_get_fabric_rcac(s_matter_controller_handle->rmaker_group_id, rcac_der, rcac_der_len), TAG,
        "Failed to fetch fabric RCAC");

    if (!s_matter_controller_handle->is_server_instance &&
        rmaker_matter_controller_set_nvs(MATTER_CTL_NVS_KEY_RCAC, rcac_der, *rcac_der_len) != ESP_OK) {
        // Still return ESP_OK if failed to store RCAC
        ESP_LOGW(TAG, "Failed to store RCAC");
    }
    return ESP_OK;
}

esp_err_t app_rmaker_matter_controller_get_stored_rcac(uint8_t *rcac_der, size_t *rcac_der_len)
{
    if (!rcac_der || !rcac_der_len) {
        return ESP_ERR_INVALID_ARG;
    }
    return rmaker_matter_controller_get_nvs(MATTER_CTL_NVS_KEY_RCAC, rcac_der, rcac_der_len);
}

esp_err_t app_rmaker_matter_controller_handle_update()
{
    return send_event_to_matter_ctl_task(MATTER_CONTROLLER_EVENT_TYPE_UPDATE_HANDLE);
}

esp_err_t app_rmaker_update_matter_device_list()
{
    return send_event_to_matter_ctl_task(MATTER_CONTROLLER_EVENT_TYPE_UPDATE_DEVICE_LIST);
}

static esp_err_t update_device_list()
{
    matter_device_t *tmp = NULL;
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(check_handle_state(), ESP_ERR_INVALID_STATE, exit, TAG, "Controller not authorized or not setup");
    ESP_GOTO_ON_ERROR(app_rmaker_api_get_matter_device_list(s_matter_controller_handle->rmaker_group_id, &tmp), exit,
                      TAG, "Failed to get matter device list");
    xSemaphoreTakeRecursive(s_matter_controller_handle->dev_list_mutex, portMAX_DELAY);
    if (s_matter_controller_handle->dev_list) {
        app_rmaker_free_matter_device_list(s_matter_controller_handle->dev_list);
    }
    s_matter_controller_handle->dev_list = tmp;
    xSemaphoreGiveRecursive(s_matter_controller_handle->dev_list_mutex);
exit:
    if (s_matter_controller_handle->dev_list_update_cb) {
        s_matter_controller_handle->dev_list_update_cb(ret);
    }
    return ret;
}

void app_rmaker_free_matter_device_list(matter_device_t *dev_list)
{
    matter_device_t *current = dev_list;
    while (current) {
        dev_list = dev_list->next;
        free(current);
        current = dev_list;
    }
}

void app_rmaker_print_matter_device_list(matter_device_t *dev_list)
{
    uint16_t dev_index = 0;
    while (dev_list) {
        ESP_LOGI(TAG, "device %d : {", dev_index);
        ESP_LOGI(TAG, "    rainmaker_node_id: %s,", dev_list->rainmaker_node_id);
        ESP_LOGI(TAG, "    matter_node_id: 0x%" PRIx32 "%" PRIx32 ",", (uint32_t)(dev_list->node_id >> 32),
                 (uint32_t)(dev_list->node_id & 0xFFFFFFFF));
        if (dev_list->is_metadata_fetched) {
            ESP_LOGI(TAG, "    is_rainmaker_device: %s,", dev_list->is_rainmaker_device ? "true" : "false");
            ESP_LOGI(TAG, "    is_online: %s,", dev_list->reachable ? "true" : "false");
            ESP_LOGI(TAG, "    endpoints : [");
            for (size_t i = 0; i < dev_list->endpoint_count; ++i) {
                ESP_LOGI(TAG, "        {");
                ESP_LOGI(TAG, "           endpoint_id: %d,", dev_list->endpoints[i].endpoint_id);
                ESP_LOGI(TAG, "           device_type_id: 0x%" PRIx32 ",", dev_list->endpoints[i].device_type_id);
                ESP_LOGI(TAG, "           device_name: %s,", dev_list->endpoints[i].device_name);
                ESP_LOGI(TAG, "        },");
            }
            ESP_LOGI(TAG, "    ]");
        }
        ESP_LOGI(TAG, "}");
        dev_list = dev_list->next;
        dev_index++;
    }
}

static matter_device_t *clone_dev_info(matter_device_t *dev)
{
    matter_device_t *ret = (matter_device_t *)MEM_CALLOC_EXTRAM(1, sizeof(matter_device_t));
    if (!ret) {
        ESP_LOGE(TAG, "Failed to allocate memory for matter device struct");
        return NULL;
    }
    memcpy(ret, dev, sizeof(matter_device_t));
    ret->next = NULL;
    return ret;
}

matter_device_t *app_rmaker_get_matter_device_list()
{
    matter_device_t *ret = NULL;
    if (!s_matter_controller_handle || !s_matter_controller_handle->dev_list_mutex) {
        ESP_LOGE(TAG, "Not initialized, call app_rmaker_matter_controller_enable first");
        return NULL;
    }
    xSemaphoreTakeRecursive(s_matter_controller_handle->dev_list_mutex, portMAX_DELAY);
    matter_device_t *current = s_matter_controller_handle->dev_list;
    while (current) {
        matter_device_t *tmp = clone_dev_info(current);
        if (!tmp) {
            app_rmaker_free_matter_device_list(ret);
            xSemaphoreGiveRecursive(s_matter_controller_handle->dev_list_mutex);
            return NULL;
        }
        tmp->next = ret;
        ret = tmp;
        current = current->next;
    }
    xSemaphoreGiveRecursive(s_matter_controller_handle->dev_list_mutex);
    return ret;
}

static void login_failure_callback(int error_code, const char *failed_reason)
{
    ESP_LOGI(TAG, "Login failed, error code: %d, reason: %s", error_code, failed_reason);
}

static void login_success_callback(void)
{
    ESP_LOGI(TAG, "Login success");
}

static void matter_ctl_auth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (!s_matter_controller_handle) {
        return;
    }
    if (event_base == RMAKER_AUTH_SERVICE_EVENT) {
        switch (event_id) {
        case RMAKER_AUTH_SERVICE_EVENT_ENABLED:
            safe_free(&s_matter_controller_handle->user_token);
            esp_rmaker_auth_service_get_user_token(&s_matter_controller_handle->user_token);
            safe_free(&s_matter_controller_handle->base_url);
            if (esp_rmaker_auth_service_get_base_url(&s_matter_controller_handle->base_url) != ESP_OK) {
                /* If failed to get base url, free the user token */
                safe_free(&s_matter_controller_handle->user_token);
            }
            break;
        case RMAKER_AUTH_SERVICE_EVENT_TOKEN_RECEIVED:
            safe_free(&s_matter_controller_handle->user_token);
            esp_rmaker_auth_service_get_user_token(&s_matter_controller_handle->user_token);
            if (s_matter_controller_handle->user_token && s_matter_controller_handle->base_url) {
                app_rmaker_matter_controller_handle_update();
            }
            break;
        case RMAKER_AUTH_SERVICE_EVENT_BASE_URL_RECEIVED:
            safe_free(&s_matter_controller_handle->base_url);
            esp_rmaker_auth_service_get_base_url(&s_matter_controller_handle->base_url);
            if (s_matter_controller_handle->user_token && s_matter_controller_handle->base_url) {
                app_rmaker_matter_controller_handle_update();
            }
            break;
        case RMAKER_AUTH_SERVICE_EVENT_DISABLED:
            break;
        default:
            break;
        }
    }
}

esp_err_t app_rmaker_matter_controller_enable(matter_controller_config_t *config)
{
    esp_err_t ret = ESP_OK;
    if (s_matter_controller_handle) {
        return ESP_OK;
    }
    if (!config || !config->setup_callback) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_STATE;
    }
    s_matter_controller_handle = (matter_controller_handle_t *)MEM_CALLOC_EXTRAM(1, sizeof(matter_controller_handle_t));
    if (!s_matter_controller_handle) {
        ESP_LOGE(TAG, "Couldn't allocate s_matter_controller_handle");
        return ESP_ERR_NO_MEM;
    }
    ESP_GOTO_ON_ERROR(
        esp_event_handler_register(RMAKER_AUTH_SERVICE_EVENT, ESP_EVENT_ANY_ID, &matter_ctl_auth_event_handler, NULL),
        exit, TAG, "Failed to register auth service event handler");
    // Ignore the return values of app_rmaker_user_api_xxx APIs, may be invoked in other places.
    app_rmaker_user_api_config_t api_config = {0};
    app_rmaker_user_api_init(&api_config);
    app_rmaker_user_api_register_login_failure_callback(login_failure_callback);
    app_rmaker_user_api_register_login_success_callback(login_success_callback);
    s_matter_controller_handle->setup_callback = config->setup_callback;
    s_matter_controller_handle->update_noc_callback = config->update_noc_callback;
    s_matter_controller_handle->dev_list_update_cb = config->device_list_update_callback;
#ifdef CONFIG_ESP_MATTER_ENABLE_MATTER_SERVER
    s_matter_controller_handle->is_server_instance = true;
#endif
    s_matter_controller_handle->service = matter_controller_setup_service_create(
        ESP_RMAKER_MATTER_CONTROLLER_SETUP_SERVICE_NAME, bulk_write_cb, NULL, NULL);
    if (!s_matter_controller_handle->service) {
        ESP_GOTO_ON_ERROR(ESP_FAIL, exit, TAG, "Failed to create Matter Controller Setup Service");
    }
    ESP_GOTO_ON_ERROR(esp_rmaker_node_add_device(esp_rmaker_get_node(), s_matter_controller_handle->service), exit, TAG,
                      "Failed to add Matter Controller Setup service");
    ESP_GOTO_ON_ERROR(nvs_flash_init_partition(MATTER_CTL_NVS_PART_NAME), exit, TAG,
                      "Failed to initialize Matter Controller NVS partition");

    s_matter_controller_handle->event_task_queue = xQueueCreate(10, sizeof(matter_controller_event_type_t));
    ESP_GOTO_ON_FALSE(s_matter_controller_handle->event_task_queue, ESP_FAIL, exit, TAG,
                      "Failed to create controller update task queue");

    // Without espressif/esp_flash_dispatcher, Tasks in SPIRAM should not call `esp_flash_xxx` functions.
    xTaskCreate(event_task_handler, "matter_ctl_task", CONFIG_RAINMAKER_MATTER_CONTROLLER_TASK_STACK, NULL, 1,
                &s_matter_controller_handle->event_task_handle);
    ESP_GOTO_ON_FALSE(s_matter_controller_handle->event_task_handle, ESP_FAIL, exit, TAG,
                      "Failed to create matter controller task");

    s_matter_controller_handle->dev_list_mutex = xSemaphoreCreateRecursiveMutex();
    ESP_GOTO_ON_FALSE(s_matter_controller_handle->dev_list_mutex, ESP_FAIL, exit, TAG,
                      "Failed to create device list mutex");

    return ESP_OK;
exit:
    if (s_matter_controller_handle->event_task_handle) {
        vTaskDelete(s_matter_controller_handle->event_task_handle);
        s_matter_controller_handle->event_task_handle = NULL;
    }
    if (s_matter_controller_handle->event_task_queue) {
        vQueueDelete(s_matter_controller_handle->event_task_queue);
        s_matter_controller_handle->event_task_queue = NULL;
    }
    if (s_matter_controller_handle->dev_list_mutex) {
        vSemaphoreDelete(s_matter_controller_handle->dev_list_mutex);
        s_matter_controller_handle->dev_list_mutex = NULL;
    }
    free(s_matter_controller_handle);
    s_matter_controller_handle = NULL;
    esp_event_handler_unregister(RMAKER_AUTH_SERVICE_EVENT, ESP_EVENT_ANY_ID, &matter_ctl_auth_event_handler);
    return ret;
}
