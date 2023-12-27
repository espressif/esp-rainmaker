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
#include <app/clusters/color-control-server/color-control-server.h>
#include <app/server/Server.h>
#include <esp_matter_console.h>
#include <app_matter.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_core.h>
#include "app_priv.h"

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::cluster;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "app_matter";
uint16_t light_endpoint_id;
extern bool rmaker_init_done;
constexpr auto k_timeout_seconds = 300;

static const char *app_matter_get_rmaker_param_name_from_id(uint32_t cluster_id, uint32_t attribute_id)
{
    if (cluster_id == OnOff::Id) {
        if (attribute_id == OnOff::Attributes::OnOff::Id) {
            return ESP_RMAKER_DEF_POWER_NAME;
        }
    } else if (cluster_id == LevelControl::Id) {
        if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
            return ESP_RMAKER_DEF_BRIGHTNESS_NAME;
        }
    } else if (cluster_id == ColorControl::Id) {
        if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
            return ESP_RMAKER_DEF_HUE_NAME;
        } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
            return ESP_RMAKER_DEF_SATURATION_NAME;
        } else if (attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
            return ESP_RMAKER_DEF_CCT_NAME;
        }
    }
    return NULL;
}

static esp_rmaker_param_val_t app_matter_get_rmaker_val(esp_matter_attr_val_t *val, uint32_t cluster_id,
                                                           uint32_t attribute_id)
{
    /* Attributes which need to be remapped */
    if (cluster_id == LevelControl::Id) {
        if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
            int value = REMAP_TO_RANGE(val->val.u8, MATTER_BRIGHTNESS, STANDARD_BRIGHTNESS);
            return esp_rmaker_int(value);
        }
    } else if (cluster_id == ColorControl::Id) {
        if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
            int value = REMAP_TO_RANGE(val->val.u8, MATTER_HUE, STANDARD_HUE);
            return esp_rmaker_int(value);
        } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
            int value = REMAP_TO_RANGE(val->val.u8, MATTER_SATURATION, STANDARD_SATURATION);
            return esp_rmaker_int(value);
        } else if (attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
            int value = REMAP_TO_RANGE_INVERSE(val->val.u16, STANDARD_TEMPERATURE_FACTOR);
            return esp_rmaker_int(value);
        }
    } else if (cluster_id == OnOff::Id) {
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

    if (type == PRE_UPDATE) {
        /* Driver update */
        if (endpoint_id == light_endpoint_id) {
            led_driver_handle_t handle = (led_driver_handle_t)priv_data;
            if (cluster_id == OnOff::Id) {
                if (attribute_id == OnOff::Attributes::OnOff::Id) {
                    err = app_driver_light_set_power(handle, val->val.b);
                }
            } else if (cluster_id == LevelControl::Id) {
                if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
                    err = app_driver_light_set_brightness(handle, REMAP_TO_RANGE(val->val.u8, MATTER_BRIGHTNESS, STANDARD_BRIGHTNESS));
                }
            } else if (cluster_id == ColorControl::Id) {
                if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
                    err = app_driver_light_set_hue(handle, REMAP_TO_RANGE(val->val.u8, MATTER_HUE, STANDARD_HUE));
                } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
                    err = app_driver_light_set_saturation(handle, REMAP_TO_RANGE(val->val.u8, MATTER_SATURATION, STANDARD_SATURATION));
                } else if (attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
                    err = app_driver_light_set_temperature(handle, REMAP_TO_RANGE_INVERSE(val->val.u16, STANDARD_TEMPERATURE_FACTOR));
                }
            }
        }
    } else if (type == POST_UPDATE) {
        if (!rmaker_init_done) {
            ESP_LOGI(TAG, "RainMaker init not done. Not processing attribute update");
            return ESP_OK;
        }

        /* RainMaker update */
        const char *device_name = LIGHT_DEVICE_NAME;
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
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "Interface IP Address changed");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "Commissioning session stopped");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        {
            ESP_LOGI(TAG, "Fabric removed successfully");
            if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0)
            {
                chip::CommissioningWindowManager & commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
                constexpr auto kTimeoutSeconds = chip::System::Clock::Seconds16(k_timeout_seconds);
                if (!commissionMgr.IsCommissioningWindowOpen())
                {
                    /* After removing last fabric, this example does not remove the Wi-Fi credentials
                     * and still has IP connectivity so, only advertising on DNS-SD.
                     */
                    CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(kTimeoutSeconds,
                                                    chip::CommissioningWindowAdvertisement::kDnssdOnly);
                    if (err != CHIP_NO_ERROR)
                    {
                        ESP_LOGE(TAG, "Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
                    }
                }
            }
        break;
        }

    case chip::DeviceLayer::DeviceEventType::kFabricWillBeRemoved:
        ESP_LOGI(TAG, "Fabric will be removed");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricUpdated:
        ESP_LOGI(TAG, "Fabric is updated");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricCommitted:
        ESP_LOGI(TAG, "Fabric is committed");
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

esp_err_t app_matter_light_create(app_driver_handle_t driver_handle)
{
    node_t *node = node::get();
    if (!node) {
        ESP_LOGE(TAG, "Matter node not found");
        return ESP_FAIL;
    }

    extended_color_light::config_t light_config;
    light_config.on_off.on_off = DEFAULT_POWER;
    light_config.on_off.lighting.start_up_on_off = nullptr;
    light_config.level_control.current_level = DEFAULT_BRIGHTNESS;
    light_config.level_control.lighting.start_up_current_level = DEFAULT_BRIGHTNESS;
    light_config.color_control.color_mode = static_cast<uint8_t>(ColorControl::ColorMode::kColorTemperature);
    light_config.color_control.enhanced_color_mode =
        static_cast<uint8_t>(ColorControlServer::EnhancedColorMode::kColorTemperature);
    light_config.color_control.color_temperature.startup_color_temperature_mireds = nullptr;
    endpoint_t *endpoint = extended_color_light::create(node, &light_config, ENDPOINT_FLAG_NONE, driver_handle);

    /* These node and endpoint handles can be used to create/add other endpoints and clusters. */
    if (!node || !endpoint) {
        ESP_LOGE(TAG, "Matter node creation failed");
    }

    light_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Light created with endpoint_id %d", light_endpoint_id);


    /* Add additional features to the node */
    cluster_t *cluster = cluster::get(endpoint, ColorControl::Id);
    cluster::color_control::feature::hue_saturation::config_t hue_saturation_config;
    hue_saturation_config.current_hue = REMAP_TO_RANGE(DEFAULT_HUE, STANDARD_HUE, MATTER_HUE);
    hue_saturation_config.current_saturation = REMAP_TO_RANGE(DEFAULT_SATURATION, STANDARD_SATURATION, MATTER_SATURATION);
    cluster::color_control::feature::hue_saturation::add(cluster, &hue_saturation_config);

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
#else
    ESP_LOGI(TAG, "Set CONFIG_ENABLE_CHIP_SHELL to enable Matter Console");
#endif
}

esp_err_t app_matter_report_power(bool val)
{
    esp_matter_attr_val_t value = esp_matter_bool(val);
    return attribute::report(light_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &value);
}

esp_err_t app_matter_report_hue(int val)
{
    esp_matter_attr_val_t value = esp_matter_uint8(REMAP_TO_RANGE(val, STANDARD_HUE, MATTER_HUE));
    return attribute::report(light_endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentHue::Id, &value);
}

esp_err_t app_matter_report_saturation(int val)
{
    esp_matter_attr_val_t value = esp_matter_uint8(REMAP_TO_RANGE(val, STANDARD_SATURATION, MATTER_SATURATION));
    return attribute::report(light_endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentSaturation::Id, &value);
}

esp_err_t app_matter_report_temperature(int val)
{
    esp_matter_attr_val_t value = esp_matter_uint16(REMAP_TO_RANGE_INVERSE(val, MATTER_TEMPERATURE_FACTOR));
    return attribute::report(light_endpoint_id, ColorControl::Id, ColorControl::Attributes::ColorTemperatureMireds::Id, &value);
}

esp_err_t app_matter_report_brightness(int val)
{
    esp_matter_attr_val_t value = esp_matter_nullable_uint8(REMAP_TO_RANGE(val, STANDARD_BRIGHTNESS, MATTER_BRIGHTNESS));
    return attribute::report(light_endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id, &value);
}
