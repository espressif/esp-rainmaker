/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <app_matter.h>
#include <app_priv.h>
#include <esp_log.h>
#include <esp_matter_console.h>
#include <esp_matter_controller_console.h>
#include <esp_matter_rainmaker.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <matter_controller_cluster.h>
#include <matter_controller_device_mgr.h>
#include <nvs_flash.h>
#include <platform/ESP32/route_hook/ESP32RouteHook.h>
#include <string.h>

#include "app_matter_ctrl.h"
#include "ui_matter_ctrl.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::cluster;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "app_matter";
static uint16_t app_endpoint_id;
extern bool rmaker_init_done;
extern bool wifi_connected;

static const char *app_matter_get_rmaker_param_name_from_id(uint32_t cluster_id, uint32_t attribute_id)
{
    if (cluster_id == OnOff::Id) {
        if (attribute_id == OnOff::Attributes::OnOff::Id) {
            return ESP_RMAKER_DEF_POWER_NAME;
        }
    }
    return NULL;
}

static esp_rmaker_param_val_t app_matter_get_rmaker_val(esp_matter_attr_val_t *val, uint32_t cluster_id,
                                                        uint32_t attribute_id)
{
    /* Attributes which need to be remapped */
    if (cluster_id == OnOff::Id) {
        if (attribute_id == OnOff::Attributes::OnOff::Id) {
            return esp_rmaker_bool(val->val.b);
        }
    }
    return esp_rmaker_int(0);
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
    esp_err_t err = ESP_OK;

    if (type == POST_UPDATE) {
        if (!rmaker_init_done) {
            ESP_LOGI(TAG, "RainMaker init not done. Not processing attribute update");
            return ESP_OK;
        }

        /* RainMaker update */
        const char *device_name = CONTROLLER_DEVICE_NAME;
        const char *param_name = app_matter_get_rmaker_param_name_from_id(cluster_id, attribute_id);
        if (!param_name) {
            ESP_LOGD(TAG, "param name not handled");
            return ESP_FAIL;
        }

        const esp_rmaker_node_t *node = esp_rmaker_get_node();
        esp_rmaker_device_t *device = esp_rmaker_node_get_device_by_name(node, device_name);
        esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_name(device, param_name);
        if (!param) {
            ESP_LOGE(TAG, "Param %s not found", param_name);
            return ESP_FAIL;
        }

        esp_rmaker_param_val_t rmaker_val = app_matter_get_rmaker_val(val, cluster_id, attribute_id);
        return esp_rmaker_param_update_and_report(param, rmaker_val);
    }

    return err;
}

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::PublicEventTypes::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;

    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        if (event->InterfaceIpAddressChanged.Type == chip::DeviceLayer::InterfaceIpChangeType::kIpV4_Assigned) {
            esp_matter::controller::device_mgr::init(0, on_device_list_update);
            wifi_connected = true;
            ui_main_status_bar_set_wifi(true);
        }
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGE(TAG, "Commissioning failed, fail safe timer expired");
        ui_matter_config_update_cb(UI_MATTER_EVT_FAILED_COMMISSION);
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        ui_matter_config_update_cb(UI_MATTER_EVT_START_COMMISSION);
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "Commissioning session stopped");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        break;

    case chip::DeviceLayer::DeviceEventType::kWiFiConnectivityChange:
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricCommitted:
        ESP_LOGI(TAG, "kFabricCommitted");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
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
    // esp_matter::controller::set_fabric_index(1);

    /* The node and endpoint handles can be used to create/add other endpoints and clusters. */
    if (!node) {
        ESP_LOGE(TAG, "Matter node creation failed");
        return ESP_FAIL;
    }
    /* Add custom matter controller cluster */
    endpoint_t *root_endpoint = esp_matter::endpoint::get(node, 0);
    esp_matter::cluster::matter_controller::create(root_endpoint, CLUSTER_FLAG_SERVER);

    /* Add custom rainmaker cluster */
    return rainmaker::init();
}

esp_err_t app_matter_endpoint_create()
{
    node_t *node = node::get();
    if (!node) {
        ESP_LOGE(TAG, "Matter node not found");
        return ESP_FAIL;
    }
    endpoint_t *endpoint = esp_matter::endpoint::create(node, ENDPOINT_FLAG_NONE, NULL);
    esp_matter::endpoint::add_device_type(endpoint, 0xFC01, 1);
    descriptor::config_t descriptor_config;
    descriptor::create(endpoint, &descriptor_config, CLUSTER_FLAG_SERVER);

    app_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Light created with endpoint_id %d", app_endpoint_id);

    return ESP_OK;
}

esp_err_t app_matter_pre_rainmaker_start()
{
    /* Other initializations for custom rainmaker cluster */
    return rainmaker::start();
}

esp_err_t app_matter_start()
{
    esp_err_t err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Matter start failed: %d", err);
    }
    return err;
}

void app_matter_enable_matter_console()
{
#if CONFIG_ENABLE_CHIP_SHELL
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::init();
#if CONFIG_ESP_MATTER_CONTROLLER_ENABLE
    esp_matter::console::controller_register_commands();
#endif
#else
    ESP_LOGI(TAG, "Set CONFIG_ENABLE_CHIP_SHELL to enable Matter Console");
#endif
}

esp_err_t app_matter_report_power(bool val)
{
    esp_matter_attr_val_t value = esp_matter_bool(val);
    return attribute::report(app_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &value);
}
