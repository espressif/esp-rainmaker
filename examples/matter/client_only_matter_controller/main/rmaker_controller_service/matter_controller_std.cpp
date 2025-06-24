/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_rmaker_core.h>
#include <matter_controller_std.h>

esp_rmaker_param_t *matter_controller_base_url_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_BASE_URL,
            esp_rmaker_str(""), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    return param;
}

esp_rmaker_param_t *matter_controller_user_token_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_USER_TOKEN,
            esp_rmaker_str(""), PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    return param;
}

esp_rmaker_param_t *matter_controller_rmaker_group_id_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_RMAKER_GROUP_ID,
            esp_rmaker_str(""), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    return param;
}

esp_rmaker_param_t *matter_controller_matter_node_id_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_MATTER_NODE_ID,
            esp_rmaker_str(""), PROP_FLAG_READ);
    return param;
}

esp_rmaker_param_t *matter_controller_matter_ctl_cmd_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_MATTER_CTL_CMD,
            esp_rmaker_int(-1), PROP_FLAG_WRITE);
    return param;
}

esp_rmaker_param_t *matter_controller_matter_ctl_status_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_MATTER_CTL_STATUS,
            esp_rmaker_int(0), PROP_FLAG_READ | PROP_FLAG_PERSIST);
    return param;
}

esp_rmaker_device_t *matter_controller_service_create(const char *serv_name, esp_rmaker_device_write_cb_t write_cb,
                                                      esp_rmaker_device_read_cb_t read_cb, void *priv_data)
{
    esp_rmaker_device_t *service = esp_rmaker_service_create(serv_name, ESP_RMAKER_SERVICE_MATTER_CONTROLLER, priv_data);
    if (service) {
        esp_rmaker_device_add_cb(service, write_cb, read_cb);
        esp_rmaker_device_add_param(service, matter_controller_base_url_param_create(ESP_RMAKER_DEF_BASE_URL_NAME));
        esp_rmaker_device_add_param(service, matter_controller_user_token_param_create(ESP_RMAKER_DEF_USER_TOKEN_NAME));
        esp_rmaker_device_add_param(service,
                    matter_controller_rmaker_group_id_param_create(ESP_RMAKER_DEF_RMAKER_GROUP_ID_NAME));
        esp_rmaker_device_add_param(service,
                    matter_controller_matter_node_id_param_create(ESP_RMAKER_DEF_MATTER_NODE_ID_NAME));
        esp_rmaker_device_add_param(service,
                    matter_controller_matter_ctl_cmd_param_create(ESP_RMAKER_DEF_MATTER_CTL_CMD_NAME));
        esp_rmaker_device_add_param(service,
                    matter_controller_matter_ctl_status_param_create(ESP_RMAKER_DEF_MATTER_CTL_STATUS_NAME));
    }
    return service;
}
