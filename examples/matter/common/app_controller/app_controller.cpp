/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <app_controller.h>
#include <app_controller_console.h>
#include <app_controller_op_creds_issuer.h>

#include <app_rmaker_matter_controller.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_matter_controller_console.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs_flash.h>

static const char *TAG = "app_controller";
static matter_controller_device_list_update_callback_t s_device_list_update_cb = NULL;
static SemaphoreHandle_t s_device_list_mutex = NULL;
static matter_device_t *s_device_list_copy = NULL;

static void app_controller_on_device_list_update(esp_err_t err, const matter_device_t *dev_list)
{
    if (err == ESP_OK) {
        matter_device_t *copy = app_rmaker_device_list_copy_create(dev_list);
        if (!copy && dev_list) {
            ESP_LOGW(TAG, "Failed to copy Matter device list for app-controller console");
        }
        xSemaphoreTake(s_device_list_mutex, portMAX_DELAY);
        app_rmaker_device_list_copy_destroy(s_device_list_copy);
        s_device_list_copy = copy;
        xSemaphoreGive(s_device_list_mutex);
    }

    if (s_device_list_update_cb) {
        s_device_list_update_cb(err, dev_list);
    }
}

matter_device_t *app_controller_device_list_copy(void)
{
    if (!s_device_list_mutex) {
        return NULL;
    }
    xSemaphoreTake(s_device_list_mutex, portMAX_DELAY);
    matter_device_t *copy = app_rmaker_device_list_copy_create(s_device_list_copy);
    xSemaphoreGive(s_device_list_mutex);
    return copy;
}

esp_err_t app_controller_set_device_params(esp_rmaker_device_t *device)
{
    ESP_RETURN_ON_FALSE(device, ESP_ERR_INVALID_ARG, TAG, "Invalid Matter Controller device");

    esp_rmaker_param_t *name_param = esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "MatterController");
    ESP_RETURN_ON_FALSE(name_param, ESP_ERR_NO_MEM, TAG, "Failed to create name param");
    ESP_RETURN_ON_ERROR(esp_rmaker_device_add_param(device, name_param), TAG, "Failed to add name param");

    return ESP_OK;
}

esp_err_t app_controller_init(matter_controller_device_list_update_callback_t callback)
{
    static bool init_done = false;

    s_device_list_update_cb = callback;

    if (init_done) {
        return ESP_OK;
    }

    s_device_list_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_device_list_mutex, ESP_ERR_NO_MEM, TAG, "Failed to create device-list mutex");

    ESP_RETURN_ON_ERROR(nvs_flash_init(), TAG, "Failed to initialize NVS");
    ESP_RETURN_ON_ERROR(esp_matter::console::controller_register_commands(), TAG,
                        "Failed to register controller console commands");
    app_controller_register_commands();
    app_controller_register_op_creds_issuer();

    matter_controller_config_t controller_config = {
        .setup_callback = app_controller_client_setup,
        .update_noc_callback = app_controller_update_noc,
        .device_list_update_callback = app_controller_on_device_list_update,
    };

    ESP_RETURN_ON_ERROR(app_rmaker_matter_controller_enable(&controller_config), TAG,
                        "Failed to enable RainMaker Matter Controller");

    init_done = true;
    ESP_LOGI(TAG, "RainMaker Matter controller initialized");
    return ESP_OK;
}
