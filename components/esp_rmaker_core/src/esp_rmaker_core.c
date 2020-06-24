// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <sdkconfig.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_ota_ops.h>

#include <esp_rmaker_core.h>
#include "esp_rmaker_internal.h"
#include "esp_rmaker_time_sync.h"
#include "esp_rmaker_storage.h"
#include "esp_rmaker_mqtt.h"
#include "esp_rmaker_claim.h"
#include "esp_rmaker_client_data.h"

static const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;

#include <esp_wifi.h>

ESP_EVENT_DEFINE_BASE(RMAKER_EVENT);

#define ESP_CLAIM_NODE_ID_SIZE  12

static const char *TAG = "esp_rmaker_core";

#define ESP_RMAKER_TASK_QUEUE_SIZE           8

#define ESP_RMAKER_TASK_STACK       CONFIG_ESP_RMAKER_TASK_STACK
#define ESP_RMAKER_TASK_PRIORITY    CONFIG_ESP_RMAKER_TASK_PRIORITY

#define ESP_RMAKER_CHECK_HANDLE(rval) \
{ \
    if (!g_ra_handle) {\
        ESP_LOGE(TAG, "ESP RainMaker not initialised"); \
        return rval; \
    } \
}
/* Handle to maintain internal information (will move to an internal file) */
typedef struct {
    char *node_id;
    esp_rmaker_node_info_t *info;
    esp_rmaker_attr_t *node_attributes;
    bool enable_time_sync;
    esp_rmaker_device_t *devices;
    esp_rmaker_device_t *device_templates;
    esp_rmaker_param_t *param_templates;
    bool rmaker_stop;
    bool mqtt_connected;
    esp_rmaker_mqtt_config_t *mqtt_config;
#ifdef CONFIG_ESP_RMAKER_SELF_CLAIM
    bool self_claim;
#endif /* CONFIG_ESP_RMAKER_SELF_CLAIM */
    QueueHandle_t work_queue;
} esp_rmaker_handle_t;
esp_rmaker_handle_t *g_ra_handle;

/* Event handler for catching system events */
static void esp_rmaker_event_handler(void* arg, esp_event_base_t event_base,
                          int event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        /* Signal rmaker thread to continue execution */
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    }
}

void esp_rmaker_deinit_handle(esp_rmaker_handle_t *rmaker_handle)
{
    if (!rmaker_handle) {
        return;
    }
    if (rmaker_handle->node_id) {
        free(rmaker_handle->node_id);
    }
    if (rmaker_handle->info) {
        if (rmaker_handle->info->name) {
            free(rmaker_handle->info->name);
        }
        if (rmaker_handle->info->type) {
            free(rmaker_handle->info->type);
        }
        if (rmaker_handle->info->fw_version) {
            free(rmaker_handle->info->fw_version);
        }
        if (rmaker_handle->info->model) {
            free(rmaker_handle->info->model);
        }
        free(rmaker_handle->info);
    }
    if (rmaker_handle->work_queue) {
        vQueueDelete(rmaker_handle->work_queue);
    }
    if (rmaker_handle->mqtt_config) {
        esp_rmaker_clean_mqtt_config(rmaker_handle->mqtt_config);
        free(rmaker_handle->mqtt_config);
    }
    free(rmaker_handle);
}

static char *esp_rmaker_populate_node_id()
{
    char *node_id = esp_rmaker_storage_get("node_id");
#ifdef CONFIG_ESP_RMAKER_SELF_CLAIM
    if (!node_id) {
        uint8_t eth_mac[6];
        esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not fetch MAC address. Please initialise Wi-Fi first");
            return NULL;
        }
        node_id = calloc(1, ESP_CLAIM_NODE_ID_SIZE + 1); /* +1 for NULL terminatation */
        snprintf(node_id, ESP_CLAIM_NODE_ID_SIZE + 1, "%02X%02X%02X%02X%02X%02X",
                eth_mac[0], eth_mac[1], eth_mac[2], eth_mac[3], eth_mac[4], eth_mac[5]);
    }
#endif /* CONFIG_ESP_RMAKER_SELF_CLAIM */
    return node_id;
}

/* Initialize ESP RainMaker */
esp_err_t esp_rmaker_init(esp_rmaker_config_t *config)
{
    if (g_ra_handle) {
        ESP_LOGE(TAG, "ESP RainMaker already initialised");
        return ESP_FAIL;
    }
    if (!config->info.name || !config->info.type) {
        ESP_LOGE(TAG, "Invalid config. Name and Type are mandatory.");
        return ESP_FAIL;
    }
    if (esp_rmaker_storage_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialise storage");
        return ESP_FAIL;
    }
    g_ra_handle = calloc(1, sizeof(esp_rmaker_handle_t));
    if (!g_ra_handle) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        return ESP_ERR_NO_MEM;
    }
    g_ra_handle->node_id = esp_rmaker_populate_node_id();
    if (!g_ra_handle->node_id) {
        ESP_LOGE(TAG, "Failed to initialise Node Id. Please perform \"claiming\" using RainMaker CLI.");
        esp_rmaker_deinit_handle(g_ra_handle);
        g_ra_handle = NULL;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Node ID ----- %s", g_ra_handle->node_id);

    g_ra_handle->info = calloc(1, sizeof(esp_rmaker_node_info_t));
    if (!g_ra_handle->info) {
        esp_rmaker_deinit_handle(g_ra_handle);
        g_ra_handle = NULL;
        ESP_LOGE(TAG, "Failed to allocate memory");
        return ESP_ERR_NO_MEM;
    }
    g_ra_handle->info->name = strdup(config->info.name);
    g_ra_handle->info->type = strdup(config->info.type);
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    if (config->info.fw_version) {
        g_ra_handle->info->fw_version = strdup(config->info.fw_version);
    } else {
        g_ra_handle->info->fw_version = strdup(app_desc->version);
    }
    if (config->info.model) {
        g_ra_handle->info->model = strdup(config->info.model);
    } else {
        g_ra_handle->info->model = strdup(app_desc->project_name);
    }
    if (!g_ra_handle->info->name || !g_ra_handle->info->type
            || !g_ra_handle->info->fw_version || !g_ra_handle->info->model) {
        esp_rmaker_deinit_handle(g_ra_handle);
        g_ra_handle = NULL;
        ESP_LOGE(TAG, "Failed to allocate memory");
        return ESP_ERR_NO_MEM;
    }

    g_ra_handle->work_queue = xQueueCreate(ESP_RMAKER_TASK_QUEUE_SIZE, sizeof(esp_rmaker_work_queue_entry_t));
    if (!g_ra_handle->work_queue) {
        esp_rmaker_deinit_handle(g_ra_handle);
        g_ra_handle = NULL;
        ESP_LOGE(TAG, "ESP RainMaker Queue Creation Failed");
        return ESP_ERR_NO_MEM;
    }
    g_ra_handle->mqtt_config = esp_rmaker_get_mqtt_config();
    if (!g_ra_handle->mqtt_config) {
#ifdef CONFIG_ESP_RMAKER_SELF_CLAIM
        g_ra_handle->self_claim = true;
        if (esp_rmaker_self_claim_init() != ESP_OK) {
            esp_rmaker_deinit_handle(g_ra_handle);
            g_ra_handle = NULL;
            ESP_LOGE(TAG, "Failed to initialise Self Claiming.");
            return ESP_FAIL;
        }
#else
        esp_rmaker_deinit_handle(g_ra_handle);
        g_ra_handle = NULL;
        ESP_LOGE(TAG, "Failed to initialise MQTT Config. Please perform \"claiming\" using RainMaker CLI.");
        return ESP_FAIL;
#endif /* !CONFIG_ESP_RMAKER_SELF_CLAIM */
    } else {
        if (esp_rmaker_mqtt_init(g_ra_handle->mqtt_config) != ESP_OK) {
            esp_rmaker_deinit_handle(g_ra_handle);
            g_ra_handle = NULL;
            ESP_LOGE(TAG, "Failed to initialise MQTT");
            return ESP_FAIL;
        }
    }
    g_ra_handle->enable_time_sync = config->enable_time_sync;
    esp_rmaker_post_event(RMAKER_EVENT_INIT_DONE, NULL, 0);
    return ESP_OK;
}

esp_err_t esp_rmaker_node_add_attribute(const char *attr_name, const char *value)
{
    ESP_RMAKER_CHECK_HANDLE(ESP_FAIL);
    if (!attr_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_rmaker_attr_t *attr = g_ra_handle->node_attributes;
    while(attr && attr->next) {
        if (strcmp(attr->name, attr_name) == 0) {
            ESP_LOGE(TAG, "Node attribute with name %s already exists", attr_name);
        }
        attr = attr->next;
    }
    esp_rmaker_attr_t *new_attr = calloc(1, sizeof(esp_rmaker_attr_t));
    if (!new_attr) {
        ESP_LOGE(TAG, "Failed to create node attribute %s", attr_name);
        return ESP_FAIL;
    }
    new_attr->name = strdup(attr_name);
    new_attr->value = strdup(value);
    if (attr) {
        attr->next = new_attr;
    } else {
        g_ra_handle->node_attributes = new_attr;
    }
    ESP_LOGI(TAG, "Node attribute %s created", attr_name);
    return ESP_OK;
}
esp_err_t esp_rmaker_add_param_template(const char *type, esp_rmaker_val_type_t val_type)
{
    ESP_RMAKER_CHECK_HANDLE(ESP_FAIL);
    esp_rmaker_param_t *param = g_ra_handle->param_templates;
    while(param && param->next) {
        if (strcmp(param->type, type) == 0) {
            ESP_LOGE(TAG, "Template for param type %s already exists", type);
        }
        param = param->next;
    }
    esp_rmaker_param_t *new_param = calloc(1, sizeof(esp_rmaker_param_t));
    if (!new_param) {
        return ESP_FAIL;
    }
    new_param->type = strdup(type);
    new_param->val.type = val_type;
    if (param) {
        param->next = new_param;
    } else {
        g_ra_handle->param_templates = new_param;
    }
    return ESP_OK;
}
esp_err_t esp_rmaker_add_dev_template(const char *type)
{
    ESP_RMAKER_CHECK_HANDLE(ESP_FAIL);
    esp_rmaker_device_t *device = g_ra_handle->device_templates;
    while(device && device->next) {
        if (strcmp(device->type, type) == 0) {
            ESP_LOGE(TAG, "Template for device type %s already exists", type);
        }
        device = device->next;
    }
    esp_rmaker_device_t *new_device = calloc(1, sizeof(esp_rmaker_device_t));
    if (!new_device) {
        return ESP_FAIL;
    }
    new_device->type = strdup(type);
    if (device) {
        device->next = new_device;
    } else {
        g_ra_handle->device_templates = new_device;
    }
    return ESP_OK;
}
esp_rmaker_device_t *esp_rmaker_get_device(const char *dev_name)
{
    ESP_RMAKER_CHECK_HANDLE(NULL);
    esp_rmaker_device_t *device = g_ra_handle->devices;
    while (device) {
        if (strcmp(device->name, dev_name) == 0) {
            break;
        }
        device = device->next;
    }
    return device;
}

esp_rmaker_device_t *esp_rmaker_get_first_device()
{
    if (g_ra_handle) {
        return g_ra_handle->devices;
    }
    return NULL;
}

esp_rmaker_device_t *esp_rmaker_get_service(const char *serv_name)
{
    return esp_rmaker_get_device(serv_name);
}

esp_rmaker_node_info_t *esp_rmaker_get_node_info()
{
    if (g_ra_handle) {
        return g_ra_handle->info;
    }
    return NULL;
}
esp_rmaker_attr_t *esp_rmaker_get_first_node_attribute()
{
    if (g_ra_handle) {
        return g_ra_handle->node_attributes;
    }
    return NULL;
}

esp_rmaker_device_t *esp_rmaker_get_first_device_template()
{
    if (g_ra_handle) {
        return g_ra_handle->device_templates;
    }
    return NULL;
}

esp_rmaker_param_t *esp_rmaker_get_first_param_template()
{
    if (g_ra_handle) {
        return g_ra_handle->param_templates;
    }
    return NULL;
}

static esp_err_t __esp_rmaker_create_device(const char *dev_name, const char *type, esp_rmaker_param_callback_t cb, void *priv_data, bool is_service)
{
    ESP_RMAKER_CHECK_HANDLE(ESP_FAIL);
    esp_rmaker_device_t *device = g_ra_handle->devices;
    while(device) {
        if (strcmp(device->name, dev_name) == 0) {
            ESP_LOGE(TAG, "Device with name %s already exists", dev_name);
            return ESP_ERR_INVALID_ARG;
        }
        if (device->next) {
            device = device->next;
        } else {
            break;
        }
    }
    esp_rmaker_device_t *new_device = calloc(1, sizeof(esp_rmaker_device_t));
    if (!new_device) {
        ESP_LOGE(TAG, "Failed to create device %s", dev_name);
        return ESP_FAIL;
    }
    new_device->name = strdup(dev_name);
    if (type) {
        new_device->type = strdup(type);
    }
    new_device->cb = cb;
    new_device->priv_data = priv_data;
    new_device->is_service = is_service;
    if (device) {
        device->next = new_device;
    } else {
        g_ra_handle->devices = new_device;
    }
    ESP_LOGD(TAG, "Device %s created", dev_name);

    return ESP_OK;
}

esp_err_t esp_rmaker_create_device(const char *dev_name, const char *type,
        esp_rmaker_param_callback_t cb, void *priv_data)
{
    return __esp_rmaker_create_device(dev_name, type, cb, priv_data, false);
}

esp_err_t esp_rmaker_create_service(const char *serv_name, const char *type,
        esp_rmaker_param_callback_t cb, void *priv_data)
{
    return __esp_rmaker_create_device(serv_name, type, cb, priv_data, true);
}

esp_rmaker_param_val_t esp_rmaker_bool(bool val)
{
    esp_rmaker_param_val_t param_val = {
        .type = RMAKER_VAL_TYPE_BOOLEAN,
        .val.b = val
    };
    return param_val;
}

esp_rmaker_param_val_t esp_rmaker_int(int val)
{
    esp_rmaker_param_val_t param_val = {
        .type = RMAKER_VAL_TYPE_INTEGER,
        .val.i = val
    };
    return param_val;
}

esp_rmaker_param_val_t esp_rmaker_float(float val)
{
    esp_rmaker_param_val_t param_val = {
        .type = RMAKER_VAL_TYPE_FLOAT,
        .val.f = val
    };
    return param_val;
}

esp_rmaker_param_val_t esp_rmaker_str(const char *val)
{
    esp_rmaker_param_val_t param_val = {
        .type = RMAKER_VAL_TYPE_STRING,
        .val.s = (char *)val
    };
    return param_val;
}

/* Add a new Device Attribute */
esp_err_t esp_rmaker_device_add_attribute(const char *dev_name, const char *attr_name, const char *val)
{
    if (!dev_name || !attr_name || !val) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_rmaker_device_t *device = esp_rmaker_get_device(dev_name);
    if (!device) {
        ESP_LOGW(TAG, "Device %s not found", dev_name);
        return ESP_ERR_INVALID_ARG;
    }
    esp_rmaker_attr_t *attr = device->attributes;
    while(attr) {
        if (strcmp(attr_name, attr->name) == 0) {
            ESP_LOGE(TAG, "Attribute with name %s already exists in Device %s", attr_name, dev_name);
            return ESP_ERR_INVALID_ARG;
        }
        if (attr->next) {
            attr = attr->next;
        } else {
            break;
        }
    }
    esp_rmaker_attr_t *new_attr = calloc(1, sizeof(esp_rmaker_attr_t));
    if (!new_attr) {
        return ESP_ERR_NO_MEM;
    }
    new_attr->name = strdup(attr_name);
    new_attr->value = strdup(val);
    if (attr) {
        attr->next = new_attr;
    } else {
        device->attributes = new_attr;
    }
    ESP_LOGD(TAG, "Device attribute %s.%s created", dev_name, attr_name);
    return ESP_OK;
}

/* Get param from names */
esp_rmaker_param_t *esp_rmaker_get_param(const char *dev_name, const char *name)
{
    if (!dev_name || !name) {
        return NULL;
    }
    ESP_RMAKER_CHECK_HANDLE(NULL);
    esp_rmaker_device_t *device = esp_rmaker_get_device(dev_name);
    if (!device) {
        return NULL;
    }
    esp_rmaker_param_t *param = device->params;
    while (param) {
        if (strcmp(name, param->name) == 0) {
            break;
        }
        param = param->next;
    }
    return param;
}

static esp_rmaker_param_t *esp_rmaker_get_param_by_val_type(const char *dev_name, const char *name, esp_rmaker_val_type_t param_type)
{
    esp_rmaker_param_t *param = esp_rmaker_get_param(dev_name, name);
    if (!param) {
        return NULL;
    }
    if (param->val.type == param_type) {
        return param;
    }
    return NULL;
}

esp_err_t esp_rmaker_device_assign_primary_param(const char *dev_name, const char *name)
{
    esp_rmaker_param_t *param = esp_rmaker_get_param(dev_name, name);
    if (param) {
        param->parent->primary = param;
        return ESP_OK;
    }
    return ESP_FAIL;
}

/* Internal. Update parameter */
static esp_err_t __esp_rmaker_update_param(const char *dev_name, const char *param_name, esp_rmaker_param_val_t val, bool external)
{
    ESP_RMAKER_CHECK_HANDLE(ESP_FAIL);
    esp_rmaker_param_t *param = esp_rmaker_get_param_by_val_type(dev_name, param_name, val.type);
    if (param) {
        switch (param->val.type) {
            case RMAKER_VAL_TYPE_STRING: {
                char *new_val = NULL;
                if (val.val.s) {
                    new_val = strdup(val.val.s);
                    if (!new_val) {
                        return ESP_FAIL;
                    }
                }
                if (param->val.val.s) {
                    free(param->val.val.s);
                }
                param->val.val.s = new_val;
                break;
            }
            case RMAKER_VAL_TYPE_BOOLEAN:
            case RMAKER_VAL_TYPE_INTEGER:
            case RMAKER_VAL_TYPE_FLOAT:
                param->val.val = val.val;
                break;
            default:
                return ESP_FAIL;
        }
        if (param->prop_flags & PROP_FLAG_PERSIST) {
            esp_rmaker_param_store_value(param);
        }
        if (external && g_ra_handle->mqtt_connected) {
            ESP_LOGD(TAG, "Value Changed for %s-%s", dev_name, param_name);
            param->flags |= RMAKER_PARAM_FLAG_VALUE_CHANGE;
            if (param->prop_flags & PROP_FLAG_TIME_SERIES) {
                esp_rmaker_report_ts_param(param);
            }
            esp_rmaker_report_param();
        }
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Parameter %s - %s of type %d not found", dev_name, param_name, val.type);
    return ESP_FAIL;
}
static esp_err_t esp_rmaker_update_param_internal(const char *dev_name, const char *param_name, esp_rmaker_param_val_t val)
{
    return __esp_rmaker_update_param(dev_name, param_name, val, false);
}
esp_err_t esp_rmaker_update_param(const char *dev_name, const char *param_name, esp_rmaker_param_val_t val)
{
    return __esp_rmaker_update_param(dev_name, param_name, val, true);
}
/* Internal. Add a new Parameter to a device */
esp_err_t esp_rmaker_device_add_param(const char *dev_name, const char *name, esp_rmaker_param_val_t val,
            uint8_t prop_flags)
{
    esp_rmaker_device_t *device = esp_rmaker_get_device(dev_name);
    if (!device) {
        ESP_LOGE(TAG, "Device %s not found", dev_name);
        return ESP_FAIL;
    }
    esp_rmaker_param_t *param = device->params;
    while(param) {
        if (strcmp(name, param->name) == 0) {
            ESP_LOGE(TAG, "Parameter with name %s already exists in Device %s", name, dev_name);
            return ESP_ERR_INVALID_ARG;
        }
        if (param->next) {
            param = param->next;
        } else {
            break;
        }
    }
    esp_rmaker_param_t *new_param = calloc(1, sizeof(esp_rmaker_param_t));
    if (!new_param) {
        return ESP_ERR_NO_MEM;
    }
    new_param->name = strdup(name);
    if (!new_param->name) {
        goto add_param_error;
    }
    new_param->val.type = val.type;
    new_param->prop_flags = prop_flags;
    new_param->parent = device;
    if (param) {
        param->next = new_param;
    } else {
        device->params = new_param;
    }
    bool stored_val_found = false;
    esp_rmaker_param_val_t stored_val;
    if (prop_flags & PROP_FLAG_PERSIST) {
        if (esp_rmaker_param_get_stored_value(new_param, &stored_val) == ESP_OK) {
            stored_val_found = true;
            val = stored_val;
        }
    }
    esp_rmaker_update_param_internal(dev_name, name, val);
    if (stored_val_found) {
        if (device->cb) {
            device->cb(device->name, new_param->name, val, device->priv_data);
        }
        if (val.type == RMAKER_VAL_TYPE_STRING) {
            if (val.val.s) {
                free(val.val.s);
            }
        }
    }
    ESP_LOGD(TAG, "Param %s - %s created", dev_name, name);
    return ESP_OK;
add_param_error:
    if (new_param) {
        if (new_param->name) {
            free(new_param->name);
        }
        free(new_param);
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t esp_rmaker_service_add_param(const char *serv_name, const char *name, esp_rmaker_param_val_t val,
            uint8_t prop_flags)
{
    return esp_rmaker_device_add_param(serv_name, name, val, prop_flags);
}

esp_err_t esp_rmaker_param_add_bounds(const char *dev_name, const char *param_name,
    esp_rmaker_param_val_t min, esp_rmaker_param_val_t max, esp_rmaker_param_val_t step)
{
    ESP_RMAKER_CHECK_HANDLE(ESP_FAIL);
    esp_rmaker_param_t *param = esp_rmaker_get_param_by_val_type(dev_name, param_name, RMAKER_VAL_TYPE_INTEGER);
    if (!param) {
        param = esp_rmaker_get_param_by_val_type(dev_name, param_name, RMAKER_VAL_TYPE_FLOAT);
        if (!param) {
            ESP_LOGE(TAG, "Numeric parameter %s - %s not found", dev_name, param_name);
            return ESP_FAIL;
        }
    }
    if ((min.type != param->val.type) || (max.type != param->val.type) || (step.type != param->val.type)) {
        ESP_LOGE(TAG, "Cannot set bounds for %s - %s because of value type mismatch", dev_name, param_name);
        return ESP_FAIL;
    }
    param->min = min;
    param->max = max;
    param->step = step;
    return ESP_OK;
}

esp_err_t esp_rmaker_param_add_type(const char *dev_name, const char *param_name, const char* type)
{
    ESP_RMAKER_CHECK_HANDLE(ESP_FAIL);
    esp_rmaker_param_t *param = esp_rmaker_get_param(dev_name, param_name);
    if (param) {
        if (param->type) {
            free(param->type);
        }
        param->type = strdup(type);
        if (param->type) {
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

esp_err_t esp_rmaker_param_add_ui_type(const char *dev_name, const char *name, const char *ui_type)
{
    ESP_RMAKER_CHECK_HANDLE(ESP_FAIL);
    esp_rmaker_param_t *param = esp_rmaker_get_param(dev_name, name);
    if (!param) {
        return ESP_FAIL;
    }
    if (param->ui_type) {
        free(param->ui_type);
    }
    if ((param->ui_type = strdup(ui_type)) != NULL ){
        return ESP_OK;
    }
    return ESP_FAIL;
}

static esp_err_t esp_rmaker_report_node_config_and_state()
{
    if (esp_rmaker_report_node_config() != ESP_OK) {
        ESP_LOGE(TAG, "Report node config failed.");
        return ESP_FAIL;
    }
    if (esp_rmaker_report_node_state() != ESP_OK) {
        ESP_LOGE(TAG, "Report node state failed.");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void __esp_rmaker_report_node_config_and_state(void *data)
{
    esp_rmaker_report_node_config_and_state();
}

esp_err_t esp_rmaker_report_node_details()
{
    return esp_rmaker_queue_work(__esp_rmaker_report_node_config_and_state, NULL);
}

void esp_rmaker_handle_work_queue()
{
    ESP_RMAKER_CHECK_HANDLE();
    esp_rmaker_work_queue_entry_t work_queue_entry;
    BaseType_t ret = xQueueReceive(g_ra_handle->work_queue, &work_queue_entry, 0);
    while (ret == pdTRUE) {
        work_queue_entry.work_fn(work_queue_entry.priv_data);
        ret = xQueueReceive(g_ra_handle->work_queue, &work_queue_entry, 0);
    }
}

static void esp_rmaker_task(void *param)
{
    ESP_RMAKER_CHECK_HANDLE();
    esp_err_t err;
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &esp_rmaker_event_handler, NULL)); 
    /* Wait for Wi-Fi connection */
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, false, true, portMAX_DELAY);

    if(g_ra_handle->enable_time_sync) {
        esp_rmaker_time_sync(); /* TODO: Error handling */
    }
#ifdef CONFIG_ESP_RMAKER_SELF_CLAIM
    if (g_ra_handle->self_claim) {
        esp_rmaker_post_event(RMAKER_EVENT_CLAIM_STARTED, NULL, 0);
        err = esp_rmaker_self_claim_perform();
        if (err != ESP_OK) {
            esp_rmaker_post_event(RMAKER_EVENT_CLAIM_FAILED, NULL, 0);
            ESP_LOGE(TAG, "esp_rmaker_self_claim_perform() returned %d. Aborting", err);
            vTaskDelete(NULL);
        }
        esp_rmaker_post_event(RMAKER_EVENT_CLAIM_SUCCESSFUL, NULL, 0);
        g_ra_handle->mqtt_config = esp_rmaker_get_mqtt_config();
        if (!g_ra_handle->mqtt_config) {
            ESP_LOGE(TAG, "Failed to initialise MQTT Config after claiming. Aborting");
            vTaskDelete(NULL);
        }
        err = esp_rmaker_mqtt_init(g_ra_handle->mqtt_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_rmaker_mqtt_init() returned %d. Aborting", err);
            vTaskDelete(NULL);
        }
    }
#endif /* CONFIG_ESP_RMAKER_SELF_CLAIM */
    err = esp_rmaker_mqtt_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_rmaker_mqtt_connect() returned %d. Aborting", err);
        vTaskDelete(NULL);
    }
    g_ra_handle->mqtt_connected = true;
    if (esp_rmaker_report_node_config_and_state() != ESP_OK) {
        ESP_LOGE(TAG, "Aborting!!!");
        goto rmaker_end;
    }
    if (esp_rmaker_register_for_get_params() != ESP_OK) {
        ESP_LOGE(TAG, "Aborting!!!");
        goto rmaker_end;
    }
    while (!g_ra_handle->rmaker_stop) {
        esp_rmaker_handle_work_queue(g_ra_handle);
        /* 2 sec delay to prevent spinning */
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
rmaker_end:
    esp_rmaker_mqtt_disconnect();
    g_ra_handle->mqtt_connected = false;
    g_ra_handle->rmaker_stop = false;
    vTaskDelete(NULL);
}

esp_err_t esp_rmaker_queue_work(esp_rmaker_work_fn_t work_fn, void *priv_data)
{
    ESP_RMAKER_CHECK_HANDLE(ESP_FAIL);
    esp_rmaker_work_queue_entry_t work_queue_entry = {
        .work_fn = work_fn,
        .priv_data = priv_data,
    };
    if (xQueueSend(g_ra_handle->work_queue, &work_queue_entry, 0) == pdTRUE) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_rmaker_handle_t *esp_rmaker_get_handle()
{
    return g_ra_handle;
}

/* Start the ESP RainMaker Core Task */
esp_err_t esp_rmaker_start()
{
    ESP_RMAKER_CHECK_HANDLE(ESP_FAIL);
    if (g_ra_handle->enable_time_sync) {
        esp_rmaker_time_sync_init();
    }
    ESP_LOGI(TAG, "Starting RainMaker Core Task");
    if (xTaskCreate(&esp_rmaker_task, "esp_rmaker_task", ESP_RMAKER_TASK_STACK,
                NULL, ESP_RMAKER_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Couldn't create RainMaker core task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t esp_rmaker_stop()
{
    ESP_RMAKER_CHECK_HANDLE(ESP_FAIL);
    g_ra_handle->rmaker_stop = true;
    return ESP_OK;
}

char *esp_rmaker_get_node_id()
{
    ESP_RMAKER_CHECK_HANDLE(NULL);
    return g_ra_handle->node_id;
}
