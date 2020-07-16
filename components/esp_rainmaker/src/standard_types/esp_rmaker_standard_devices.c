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


#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>

esp_err_t esp_rmaker_create_switch_device(const char *dev_name,
        esp_rmaker_param_callback_t cb, void *priv_data, bool power)
{
    esp_err_t err = esp_rmaker_create_device(dev_name, ESP_RMAKER_DEVICE_SWITCH, cb, priv_data);
    if (err == ESP_OK) {
        esp_rmaker_device_add_name_param(dev_name, ESP_RMAKER_DEF_NAME_PARAM);
        esp_rmaker_device_add_power_param(dev_name, ESP_RMAKER_DEF_POWER_NAME, power);
        esp_rmaker_device_assign_primary_param(dev_name, ESP_RMAKER_DEF_POWER_NAME);
    }
    return err;
}

esp_err_t esp_rmaker_create_lightbulb_device(const char *dev_name,
        esp_rmaker_param_callback_t cb, void *priv_data, bool power)
{
    esp_err_t err = esp_rmaker_create_device(dev_name, ESP_RMAKER_DEVICE_LIGHTBULB, cb, priv_data);
    if (err == ESP_OK) {
        esp_rmaker_device_add_name_param(dev_name, ESP_RMAKER_DEF_NAME_PARAM);
        esp_rmaker_device_add_power_param(dev_name, ESP_RMAKER_DEF_POWER_NAME, power);
        esp_rmaker_device_assign_primary_param(dev_name, ESP_RMAKER_DEF_POWER_NAME);
    }
    return err;
}

esp_err_t esp_rmaker_create_fan_device(const char *dev_name,
        esp_rmaker_param_callback_t cb, void *priv_data, bool power)
{
    esp_err_t err = esp_rmaker_create_device(dev_name, ESP_RMAKER_DEVICE_FAN, cb, priv_data);
    if (err == ESP_OK) {
        esp_rmaker_device_add_name_param(dev_name, ESP_RMAKER_DEF_NAME_PARAM);
        esp_rmaker_device_add_power_param(dev_name, ESP_RMAKER_DEF_POWER_NAME, power);
        esp_rmaker_device_assign_primary_param(dev_name, ESP_RMAKER_DEF_POWER_NAME);
    }
    return err;
}

esp_err_t esp_rmaker_create_temp_sensor_device(const char *dev_name,
        esp_rmaker_param_callback_t cb, void *priv_data, float temperature)
{
    esp_err_t err = esp_rmaker_create_device(dev_name, ESP_RMAKER_DEVICE_TEMP_SENSOR, cb, priv_data);
    if (err == ESP_OK) {
        esp_rmaker_device_add_name_param(dev_name, ESP_RMAKER_DEF_NAME_PARAM);
        esp_rmaker_device_add_temperature_param(dev_name, ESP_RMAKER_DEF_TEMPERATURE_NAME, temperature);
        esp_rmaker_device_assign_primary_param(dev_name, ESP_RMAKER_DEF_TEMPERATURE_NAME);
    }
    return err;
}
