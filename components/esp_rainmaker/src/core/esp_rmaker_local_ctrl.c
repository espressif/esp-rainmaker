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

#include <stdlib.h>
#include <esp_log.h>
#include <esp_local_ctrl.h>
#include <esp_rmaker_internal.h>
#include <esp_https_server.h>
#include <mdns.h>

static const char * TAG = "esp_rmaker_local";

/* Random Port number that will be used by the local control http instance
 * for internal control communication.
 */
#define ESP_RMAKER_LOCAL_CTRL_HTTP_CTRL_PORT    12312

/* Custom allowed property types */
enum property_types {
    PROP_TYPE_NODE_CONFIG = 1,
    PROP_TYPE_NODE_PARAMS,
};

/* Custom flags that can be set for a property */
enum property_flags {
    PROP_FLAG_READONLY = (1 << 0)
};

/********* Handler functions for responding to control requests / commands *********/

static esp_err_t get_property_values(size_t props_count,
                                     const esp_local_ctrl_prop_t props[],
                                     esp_local_ctrl_prop_val_t prop_values[],
                                     void *usr_ctx)
{
    esp_err_t ret = ESP_OK;
    uint32_t i;
    for (i = 0; i < props_count && ret == ESP_OK ; i++) {
        ESP_LOGD(TAG, "(%d) Reading property : %s", i, props[i].name);
        switch (props[i].type) {
            case PROP_TYPE_NODE_CONFIG: {
                char *node_config = esp_rmaker_get_node_config();
                if (!node_config) {
                    ESP_LOGE(TAG, "Failed to allocate memory for %s", props[i].name);
                    ret = ESP_ERR_NO_MEM;
                } else {
                    prop_values[i].size = strlen(node_config);
                    prop_values[i].data = node_config;
                    prop_values[i].free_fn = free;
                }
                break;
            }
            case PROP_TYPE_NODE_PARAMS: {
                char *node_params = esp_rmaker_get_node_params();
                if (!node_params) {
                    ESP_LOGE(TAG, "Failed to allocate memory for %s", props[i].name);
                    ret = ESP_ERR_NO_MEM;
                } else {
                    prop_values[i].size = strlen(node_params);
                    prop_values[i].data = node_params;
                    prop_values[i].free_fn = free;
                }
                break;
            }
            default:
                break;
        }
    }
    if (ret != ESP_OK) {
        for (uint32_t j = 0; j <= i; j++) {
            if (prop_values[j].free_fn) {
                ESP_LOGI(TAG, "Freeing memory for %s", props[j].name);
                prop_values[j].free_fn(prop_values[j].data);
                prop_values[j].free_fn = NULL;
                prop_values[j].data = NULL;
                prop_values[j].size = 0;
            }
        }
    }
    return ret;
}

static esp_err_t set_property_values(size_t props_count,
                                     const esp_local_ctrl_prop_t props[],
                                     const esp_local_ctrl_prop_val_t prop_values[],
                                     void *usr_ctx)
{
    esp_err_t ret = ESP_OK;
    uint32_t i;
    /* First check if any of the properties are read-only properties. If found, just abort */
    for (i = 0; i < props_count; i++) {
        /* Cannot set the value of a read-only property */
        if (props[i].flags & PROP_FLAG_READONLY) {
            ESP_LOGE(TAG, "%s is read-only", props[i].name);
            return ESP_ERR_INVALID_ARG;
        }
    }
    for (i = 0; i < props_count && ret == ESP_OK; i++) {
        switch (props[i].type) {
            case PROP_TYPE_NODE_PARAMS:
                ret = esp_rmaker_handle_set_params((char *)prop_values[i].data,
                        prop_values[i].size, ESP_RMAKER_REQ_SRC_LOCAL);
                break;
            default:
                break;
        }
    }
    return ret;
}

esp_err_t esp_rmaker_start_local_ctrl_service(const char *serv_name)
{
    ESP_LOGI(TAG, "Starting ESP Local control with HTTP Transport.");
    /* Set the configuration */
    static httpd_ssl_config_t https_conf = HTTPD_SSL_CONFIG_DEFAULT();
    https_conf.transport_mode = HTTPD_SSL_TRANSPORT_INSECURE;
    https_conf.port_insecure = CONFIG_ESP_RMAKER_LOCAL_CTRL_HTTP_PORT;
    https_conf.httpd.ctrl_port = ESP_RMAKER_LOCAL_CTRL_HTTP_CTRL_PORT;
    
    mdns_init();
    mdns_hostname_set(serv_name);

    esp_local_ctrl_config_t config = {
        .transport = ESP_LOCAL_CTRL_TRANSPORT_HTTPD,
        .transport_config = {
            .httpd = &https_conf
        },
        .handlers = {
            /* User defined handler functions */
            .get_prop_values = get_property_values,
            .set_prop_values = set_property_values,
            .usr_ctx         = NULL,
            .usr_ctx_free_fn = NULL
        },
        /* Maximum number of properties that may be set */
        .max_properties = 10
    };

    /* Start esp_local_ctrl service */
    ESP_ERROR_CHECK(esp_local_ctrl_start(&config));
    ESP_LOGI(TAG, "esp_local_ctrl service started with name : %s", serv_name);

    /* Create the Node Config property */
    esp_local_ctrl_prop_t node_config = {
        .name        = "config",
        .type        = PROP_TYPE_NODE_CONFIG,
        .size        = 0,
        .flags       = PROP_FLAG_READONLY,
        .ctx         = NULL,
        .ctx_free_fn = NULL
    };

    /* Create the Node Params property */
    esp_local_ctrl_prop_t node_params = {
        .name        = "params",
        .type        = PROP_TYPE_NODE_PARAMS,
        .size        = 0,
        .flags       = 0,
        .ctx         = NULL,
        .ctx_free_fn = NULL
    };

    /* Now register the properties */
    ESP_ERROR_CHECK(esp_local_ctrl_add_property(&node_config));
    ESP_ERROR_CHECK(esp_local_ctrl_add_property(&node_params));
    return ESP_OK;
}
