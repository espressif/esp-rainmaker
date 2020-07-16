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

#include <stdint.h>
#include <esp_err.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>

esp_err_t esp_rmaker_device_add_name_param(const char *dev_name, const char *param_name)
{
    esp_err_t err = esp_rmaker_device_add_param(dev_name, param_name,
            esp_rmaker_str(dev_name), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    if (err == ESP_OK) {
        esp_rmaker_param_add_type(dev_name, param_name, ESP_RMAKER_PARAM_NAME);
    }
    return err;
}

esp_err_t esp_rmaker_device_add_power_param(const char *dev_name, const char *param_name, bool val)
{
    esp_err_t err = esp_rmaker_device_add_param(dev_name, param_name,
            esp_rmaker_bool(val), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (err == ESP_OK) {
        esp_rmaker_param_add_ui_type(dev_name, param_name, ESP_RMAKER_UI_TOGGLE);
        esp_rmaker_param_add_type(dev_name, param_name, ESP_RMAKER_PARAM_POWER);
    }
    return err;
}

esp_err_t esp_rmaker_device_add_brightness_param(const char *dev_name, const char *param_name, int val)
{
    esp_err_t err = esp_rmaker_device_add_param(dev_name, param_name,
            esp_rmaker_int(val), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (err == ESP_OK) {
        esp_rmaker_param_add_ui_type(dev_name, param_name, ESP_RMAKER_UI_SLIDER);
        esp_rmaker_param_add_type(dev_name, param_name, ESP_RMAKER_PARAM_BRIGHTNESS);
        esp_rmaker_param_add_bounds(dev_name, param_name, esp_rmaker_int(0), esp_rmaker_int(100), esp_rmaker_int(1));
    }
    return err;
}

esp_err_t esp_rmaker_device_add_hue_param(const char *dev_name, const char *param_name, int val)
{
    esp_err_t err = esp_rmaker_device_add_param(dev_name, param_name,
            esp_rmaker_int(val), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (err == ESP_OK) {
        esp_rmaker_param_add_ui_type(dev_name, param_name, ESP_RMAKER_UI_SLIDER);
        esp_rmaker_param_add_type(dev_name, param_name, ESP_RMAKER_PARAM_HUE);
        esp_rmaker_param_add_bounds(dev_name, param_name, esp_rmaker_int(0), esp_rmaker_int(360), esp_rmaker_int(1));
    }
    return err;
}

esp_err_t esp_rmaker_device_add_saturation_param(const char *dev_name, const char *param_name, int val)
{
    esp_err_t err = esp_rmaker_device_add_param(dev_name, param_name,
            esp_rmaker_int(val), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (err == ESP_OK) {
        esp_rmaker_param_add_ui_type(dev_name, param_name, ESP_RMAKER_UI_SLIDER);
        esp_rmaker_param_add_type(dev_name, param_name, ESP_RMAKER_PARAM_SATURATION);
        esp_rmaker_param_add_bounds(dev_name, param_name, esp_rmaker_int(0), esp_rmaker_int(100), esp_rmaker_int(1));
    }
    return err;
}

esp_err_t esp_rmaker_device_add_intensity_param(const char *dev_name, const char *param_name, int val)
{
    esp_err_t err = esp_rmaker_device_add_param(dev_name, param_name,
            esp_rmaker_int(val), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (err == ESP_OK) {
        esp_rmaker_param_add_ui_type(dev_name, param_name, ESP_RMAKER_UI_SLIDER);
        esp_rmaker_param_add_type(dev_name, param_name, ESP_RMAKER_PARAM_INTENSITY);
        esp_rmaker_param_add_bounds(dev_name, param_name, esp_rmaker_int(0), esp_rmaker_int(100), esp_rmaker_int(1));
    }
    return err;
}

esp_err_t esp_rmaker_device_add_cct_param(const char *dev_name, const char *param_name, int val)
{
    esp_err_t err = esp_rmaker_device_add_param(dev_name, param_name,
            esp_rmaker_int(val), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (err == ESP_OK) {
        esp_rmaker_param_add_ui_type(dev_name, param_name, ESP_RMAKER_UI_SLIDER);
        esp_rmaker_param_add_type(dev_name, param_name, ESP_RMAKER_PARAM_CCT);
        esp_rmaker_param_add_bounds(dev_name, param_name, esp_rmaker_int(2700), esp_rmaker_int(6500), esp_rmaker_int(100));
    }
    return err;
}

esp_err_t esp_rmaker_device_add_direction_param(const char *dev_name, const char *param_name, int val)
{
    esp_err_t err = esp_rmaker_device_add_param(dev_name, param_name,
            esp_rmaker_int(val), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (err == ESP_OK) {
        esp_rmaker_param_add_ui_type(dev_name, param_name, ESP_RMAKER_UI_DROPDOWN);
        esp_rmaker_param_add_type(dev_name, param_name, ESP_RMAKER_PARAM_DIRECTION);
        esp_rmaker_param_add_bounds(dev_name, param_name, esp_rmaker_int(0), esp_rmaker_int(1), esp_rmaker_int(1));
    }
    return err;
}

esp_err_t esp_rmaker_device_add_speed_param(const char *dev_name, const char *param_name, int val)
{
    esp_err_t err = esp_rmaker_device_add_param(dev_name, param_name,
            esp_rmaker_int(val), PROP_FLAG_READ | PROP_FLAG_WRITE);
    if (err == ESP_OK) {
        esp_rmaker_param_add_ui_type(dev_name, param_name, ESP_RMAKER_UI_SLIDER);
        esp_rmaker_param_add_type(dev_name, param_name, ESP_RMAKER_PARAM_SPEED);
        esp_rmaker_param_add_bounds(dev_name, param_name, esp_rmaker_int(0), esp_rmaker_int(5), esp_rmaker_int(1));
    }
    return err;
}

esp_err_t esp_rmaker_device_add_temperature_param(const char *dev_name, const char *param_name, float val)
{
    esp_err_t err = esp_rmaker_device_add_param(dev_name, param_name,
            esp_rmaker_float(val), PROP_FLAG_READ);
    if (err == ESP_OK) {
        esp_rmaker_param_add_type(dev_name, param_name, ESP_RMAKER_PARAM_TEMPERATURE);
    }
    return err;
}

esp_err_t esp_rmaker_service_add_ota_status_param(const char *serv_name, const char *param_name)
{
    esp_err_t err = esp_rmaker_service_add_param(serv_name, param_name,
            esp_rmaker_str(""), PROP_FLAG_READ);
    if (err == ESP_OK) {
        esp_rmaker_param_add_type(serv_name, param_name, ESP_RMAKER_PARAM_OTA_STATUS);
    }
    return err;
}

esp_err_t esp_rmaker_service_add_ota_info_param(const char *serv_name, const char *param_name)
{
    esp_err_t err = esp_rmaker_service_add_param(serv_name, param_name,
            esp_rmaker_str(""), PROP_FLAG_READ);
    if (err == ESP_OK) {
        esp_rmaker_param_add_type(serv_name, param_name, ESP_RMAKER_PARAM_OTA_INFO);
    }
    return err;
}

esp_err_t esp_rmaker_service_add_ota_url_param(const char *serv_name, const char *param_name)
{
    esp_err_t err = esp_rmaker_service_add_param(serv_name, param_name,
            esp_rmaker_str(""), PROP_FLAG_WRITE);
    if (err == ESP_OK) {
        esp_rmaker_param_add_type(serv_name, param_name, ESP_RMAKER_PARAM_OTA_URL);
    }
    return err;
}
