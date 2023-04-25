/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <string.h>
#include <led_driver.h>
#include <esp_matter_rainmaker.h>
#include <platform/ESP32/route_hook/ESP32RouteHook.h>
#include <esp_matter_console.h>
#include <app_matter.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_core.h>
#include <app_priv.h>

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::cluster;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "app_matter";
uint16_t switch_endpoint_id;

esp_err_t app_matter_send_command_binding(bool power)
{
    client::command_handle_t command;
    command.cluster_id = OnOff::Id;
    if (power == true) {
        command.command_id = OnOff::Commands::On::Id;
    } else {
        command.command_id = OnOff::Commands::Off::Id;
    }

    lock::chip_stack_lock(portMAX_DELAY);
    esp_err_t err = client::cluster_update(switch_endpoint_id, &command);
    lock::chip_stack_unlock();
    return err;
}

static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %d, effect: %d", type, effect_id);
    return ESP_OK;
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    return ESP_OK;
}

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::PublicEventTypes::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;

    default:
        break;
    }
}


esp_err_t app_matter_init()
{
    /* Create a Matter node */
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);

    /* The node and endpoint handles can be used to create/add other endpoints and clusters. */
    if (!node) {
        ESP_LOGE(TAG, "Matter node creation failed");
        return ESP_FAIL;
    }

    /* Add custom rainmaker cluster */
    return rainmaker::init();
}

static void app_matter_client_command_callback(client::peer_device_t *peer_device, client::command_handle_t *command_handle,
                                        void *priv_data)
{
    if (command_handle->cluster_id == OnOff::Id) {
                /* RainMaker update */

        const esp_rmaker_node_t *node = esp_rmaker_get_node();
        esp_rmaker_device_t *device = esp_rmaker_node_get_device_by_name(node, SWITCH_DEVICE_NAME);
        esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_name(device, ESP_RMAKER_DEF_POWER_NAME);

        if (command_handle->command_id == OnOff::Commands::Off::Id) {
            on_off::command::send_off(peer_device, command_handle->endpoint_id);
            app_driver_switch_set_power((app_driver_handle_t)priv_data, false);
            esp_rmaker_param_update_and_report(param, esp_rmaker_bool(false));
        } else if (command_handle->command_id == OnOff::Commands::On::Id) {
            on_off::command::send_on(peer_device, command_handle->endpoint_id);
            app_driver_switch_set_power((app_driver_handle_t)priv_data, true);
            esp_rmaker_param_update_and_report(param, esp_rmaker_bool(true));
        } else if (command_handle->command_id == OnOff::Commands::Toggle::Id) {
            on_off::command::send_toggle(peer_device, command_handle->endpoint_id);
            esp_rmaker_param_val_t *param_val = esp_rmaker_param_get_val(param);
            app_driver_switch_set_power((app_driver_handle_t)priv_data, !param_val->val.b);
            esp_rmaker_param_update_and_report(param, esp_rmaker_bool(!param_val->val.b));
        }
    } else if (command_handle->cluster_id == Identify::Id) {
        if (((char *)command_handle->command_data)[0] != 1) {
                ESP_LOGE(TAG, "Number of parameters error");
                return;
            }
        identify::command::send_identify(peer_device, command_handle->endpoint_id,
                                            strtoul((const char *)(command_handle->command_data) + 1, NULL, 16));
    }
}

esp_err_t app_matter_switch_create(app_driver_handle_t driver_handle)
{
    node_t *node = node::get();
    if (!node) {
        ESP_LOGE(TAG, "Matter node not found");
        return ESP_FAIL;
    }

    on_off_switch::config_t switch_config;
    endpoint_t *endpoint = on_off_switch::create(node, &switch_config, ENDPOINT_FLAG_NONE, driver_handle);
    if (!endpoint) {
        ESP_LOGE(TAG, "Matter endpoint creation failed");
        return ESP_FAIL;
    }
    cluster::groups::config_t groups_config;
    cluster::groups::create(endpoint, &groups_config, CLUSTER_FLAG_SERVER | CLUSTER_FLAG_CLIENT);

    switch_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Switch created with endpoint_id %d", switch_endpoint_id);
    return ESP_OK;
}

esp_err_t app_matter_pre_rainmaker_start()
{
    /* Other initializations for custom rainmaker cluster */
    return rainmaker::start();
}

esp_err_t app_matter_start()
{
    client::set_command_callback(app_matter_client_command_callback, NULL, NULL);
    esp_err_t err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Matter start failed: %d", err);
    }
    app_matter_send_command_binding(DEFAULT_POWER);
    return err;
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
