/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include<esp_matter.h>
#include <esp_matter_rainmaker.h>
#include <esp_matter_console.h>

using namespace esp_matter;

static const char *TAG = "app_matter";

esp_err_t app_matter_init(attribute::callback_t app_attribute_update_cb, identification::callback_t app_identification_cb)
{
    /* Create a Matter node */
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);

    /* The node and endpoint handles can be used to create/add other endpoints and clusters. */
    if (!node) {
        ESP_LOGE(TAG, "Matter node creation failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t app_matter_start(event_callback_t app_event_cb)
{
    esp_err_t err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Matter start failed: %d", err);
    }
    return err;
}

esp_err_t app_matter_rmaker_init()
{
    /* Add custom rainmaker cluster */
    return rainmaker::init();
}

esp_err_t app_matter_rmaker_start()
{
    /* Other initializations for custom rainmaker cluster */
    return rainmaker::start();
}

void app_matter_enable_matter_console()
{
#if CONFIG_ENABLE_CHIP_SHELL
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::init();
#else
    ESP_LOGI(TAG, "Set CONFIG_ENABLE_CHIP_SHELL to enable Matter Console");
#endif
}