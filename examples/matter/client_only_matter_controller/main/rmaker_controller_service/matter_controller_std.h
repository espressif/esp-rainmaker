/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_err.h>
#include <esp_rmaker_core.h>

// Matter Controller device
#define ESP_RMAKER_DEVICE_MATTER_CONTROLLER     "esp.device.matter-controller"

// Matter Controller service
#define ESP_RMAKER_SERVICE_MATTER_CONTROLLER    "esp.service.matter-controller"

// Matter Controller parameters
#define ESP_RMAKER_DEF_BASE_URL_NAME            "BaseURL"
#define ESP_RMAKER_PARAM_BASE_URL               "esp.param.base-url"
#define ESP_RMAKER_DEF_USER_TOKEN_NAME          "UserToken"
#define ESP_RMAKER_PARAM_USER_TOKEN             "esp.param.user-token"
#define ESP_RMAKER_DEF_RMAKER_GROUP_ID_NAME     "RMakerGroupID"
#define ESP_RMAKER_PARAM_RMAKER_GROUP_ID        "esp.param.rmaker-group-id"
#define ESP_RMAKER_DEF_MATTER_NODE_ID_NAME      "MatterNodeID"
#define ESP_RMAKER_PARAM_MATTER_NODE_ID         "esp.param.matter-node-id"
#define ESP_RMAKER_DEF_MATTER_CTL_CMD_NAME      "MTCtlCMD"
#define ESP_RMAKER_PARAM_MATTER_CTL_CMD         "esp.param.matter-ctl-cmd"
#define ESP_RMAKER_DEF_MATTER_CTL_STATUS_NAME   "MTCtlStatus"
#define ESP_RMAKER_PARAM_MATTER_CTL_STATUS      "esp.param.matter-ctl-status"

esp_rmaker_param_t *matter_controller_base_url_param_create(const char *param_name);
esp_rmaker_param_t *matter_controller_user_token_param_create(const char *param_name);
esp_rmaker_param_t *matter_controller_rmaker_group_id_param_create(const char *param_name);
esp_rmaker_param_t *matter_controller_matter_node_id_param_create(const char *param_name);
esp_rmaker_param_t *matter_controller_matter_ctl_cmd_param_create(const char *param_name);
esp_rmaker_param_t *matter_controller_matter_ctl_status_param_create(const char *param_name);

esp_rmaker_device_t *matter_controller_service_create(const char *serv_name, esp_rmaker_device_write_cb_t write_cb,
                                                      esp_rmaker_device_read_cb_t read_cb, void *priv_data);
