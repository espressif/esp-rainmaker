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
#include <app_matter_switch.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_core.h>
#include <esp_matter_client.h>
#include <lib/core/Optional.h>
#include <app_matter.h>
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
    client::request_handle_t req_handle;
    req_handle.type = esp_matter::client::INVOKE_CMD;
    req_handle.command_path.mClusterId = OnOff::Id;
    if (power == true) {
        req_handle.command_path.mCommandId = OnOff::Commands::On::Id;
    } else {
        req_handle.command_path.mCommandId = OnOff::Commands::Off::Id;
    }

    lock::chip_stack_lock(portMAX_DELAY);
    esp_err_t err = client::cluster_update(switch_endpoint_id, &req_handle);
    lock::chip_stack_unlock();
    return err;
}

esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %d, effect: %d", type, effect_id);
    return ESP_OK;
}

esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    return ESP_OK;
}

void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::PublicEventTypes::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;

    case chip::DeviceLayer::DeviceEventType::PublicEventTypes::kBLEDeinitialized:
        ESP_LOGI(TAG, "BLE deinitialized and memory reclaimed");
        // Starting RainMaker after Matter commissioning is complete
        // and BLE memory is reclaimed, so that MQTT connect doesn't fail.
        esp_rmaker_start();
        break;

    default:
        break;
    }
}


static void send_command_success_callback(void *context, const ConcreteCommandPath &command_path,
                                          const chip::app::StatusIB &status, TLVReader *response_data)
{
    ESP_LOGI(TAG, "Send command success");
}

static void send_command_failure_callback(void *context, CHIP_ERROR error)
{
    ESP_LOGI(TAG, "Send command failure: err :%" CHIP_ERROR_FORMAT, error.Format());
}

void app_matter_client_command_callback(client::peer_device_t *peer_device, client::request_handle_t *req_handle,
                                        void *priv_data)
{

    if (req_handle->type != esp_matter::client::INVOKE_CMD) {
        return;
    }
    char command_data_str[32];
    if (req_handle->command_path.mClusterId == OnOff::Id) {
                /* RainMaker update */
        strcpy(command_data_str, "{}");
        const esp_rmaker_node_t *node = esp_rmaker_get_node();
        esp_rmaker_device_t *device = esp_rmaker_node_get_device_by_name(node, SWITCH_DEVICE_NAME);
        esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_name(device, ESP_RMAKER_DEF_POWER_NAME);

        if (req_handle->command_path.mCommandId == OnOff::Commands::Off::Id) {
            app_driver_switch_set_power((app_driver_handle_t)priv_data, false);
            esp_rmaker_param_update_and_report(param, esp_rmaker_bool(false));
        } else if (req_handle->command_path.mCommandId == OnOff::Commands::On::Id) {
            app_driver_switch_set_power((app_driver_handle_t)priv_data, true);
            esp_rmaker_param_update_and_report(param, esp_rmaker_bool(true));
        } else if (req_handle->command_path.mCommandId == OnOff::Commands::Toggle::Id) {
            esp_rmaker_param_val_t *param_val = esp_rmaker_param_get_val(param);
            app_driver_switch_set_power((app_driver_handle_t)priv_data, !param_val->val.b);
            esp_rmaker_param_update_and_report(param, esp_rmaker_bool(!param_val->val.b));
        }

    } else if (req_handle->command_path.mClusterId == Identify::Id) {
        if (req_handle->command_path.mCommandId == Identify::Commands::Identify::Id) {
            if (((char *)req_handle->request_data)[0] != 1) {
                    ESP_LOGE(TAG, "Number of parameters error");
                    return;
                }
                sprintf(command_data_str, "{\"0:U16\": %ld}",
                        strtoul((const char *)(req_handle->request_data) + 1, NULL, 16));
        }else {
                ESP_LOGE(TAG, "Unsupported command");
                return;
            }
        }else {
            ESP_LOGE(TAG, "Unsupported cluster");
            return;
        }

    client::interaction::invoke::send_request(NULL, peer_device, req_handle->command_path, command_data_str,
                                              send_command_success_callback, send_command_failure_callback,
                                              chip::NullOptional);

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
