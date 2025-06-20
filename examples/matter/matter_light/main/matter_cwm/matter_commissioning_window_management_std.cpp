/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "matter_commissioning_window_management_std.h"
#include "esp_rmaker_core.h"

static esp_rmaker_param_t *matter_qrcode_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_MATTER_QRCODE,
                                                        esp_rmaker_str(""), PROP_FLAG_READ);
    return param;
}

static esp_rmaker_param_t *matter_manualcode_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_MATTER_MANUALCODE,
                                                        esp_rmaker_str(""), PROP_FLAG_READ);
    return param;
}

static esp_rmaker_param_t *matter_commissioning_window_open_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_MATTER_COMMISSIONING_WINDOW_OPEN,
                                                        esp_rmaker_bool(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
    return param;
}

esp_rmaker_param_t *matter_commissioning_window_management_service_create(
    const char *serv_name, esp_rmaker_device_write_cb_t write_cb, esp_rmaker_device_read_cb_t read_cb, void *priv_data)
{
    esp_rmaker_device_t *service = esp_rmaker_service_create(serv_name,
                                                             ESP_RMAKER_SERVICE_MATTER_COMMISSIONING_WINDOW_MANAGEMENT,
                                                             priv_data);
    if (service) {
        esp_rmaker_device_add_cb(service, write_cb, read_cb);
#ifdef CONFIG_CUSTOM_COMMISSIONABLE_DATA_PROVIDER
        esp_rmaker_device_add_param(service, matter_qrcode_param_create(ESP_RMAKER_DEF_MATTER_QRCODE_NAME));
        esp_rmaker_device_add_param(service, matter_manualcode_param_create(ESP_RMAKER_DEF_MATTER_MANUALCODE_NAME));
#endif
        esp_rmaker_device_add_param(service, matter_commissioning_window_open_param_create(ESP_RMAKER_DEF_MATTER_COMMISSIONING_WINDOW_OPEN_NAME));
    }
    return service;
}
