/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <sdkconfig.h>
#include <stdlib.h>
#include <string.h>

#include <app_end_device.h>
#include <esp_matter.h>
#include <iot_button.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <app_priv.h>
#include <app_matter_light.h>

using namespace esp_matter;
using namespace chip::app::Clusters;

static const char *TAG = "app_driver";
extern uint16_t light_endpoint_id;

static bool current_power;
static uint8_t current_brightness;
static uint16_t current_hue;
static uint8_t current_saturation;
static bool current_temperature_mode;
static uint8_t current_temperature_red;
static uint8_t current_temperature_green;
static uint8_t current_temperature_blue;

static esp_err_t app_driver_light_refresh(app_driver_handle_t handle)
{
    led_indicator_handle_t led = static_cast<led_indicator_handle_t>(handle);

    if (!current_power || current_brightness == 0) {
        return app_end_device_led_set_rgb(led, 0, 0, 0);
    }

    uint16_t red = 0;
    uint16_t green = 0;
    uint16_t blue = 0;

    if (current_temperature_mode) {
        red = (current_temperature_red * current_brightness) / 100;
        green = (current_temperature_green * current_brightness) / 100;
        blue = (current_temperature_blue * current_brightness) / 100;
    } else {
        uint16_t hue = current_hue % 360;
        uint16_t value = (current_brightness * 255) / 100;
        uint16_t chroma = (value * current_saturation) / 100;
        uint16_t hue_mod = hue % 120;
        uint16_t distance = hue_mod > 60 ? hue_mod - 60 : 60 - hue_mod;
        uint16_t secondary = (chroma * (60 - distance)) / 60;
        uint16_t minimum = value - chroma;

        if (hue < 60) {
            red = chroma;
            green = secondary;
        } else if (hue < 120) {
            red = secondary;
            green = chroma;
        } else if (hue < 180) {
            green = chroma;
            blue = secondary;
        } else if (hue < 240) {
            green = secondary;
            blue = chroma;
        } else if (hue < 300) {
            red = secondary;
            blue = chroma;
        } else {
            red = chroma;
            blue = secondary;
        }

        red += minimum;
        green += minimum;
        blue += minimum;
    }

    return app_end_device_led_set_rgb(led, red, green, blue);
}

esp_err_t app_driver_light_set_power(app_driver_handle_t handle, bool value)
{
    current_power = value;
    return app_driver_light_refresh(handle);
}

esp_err_t app_driver_light_set_brightness(app_driver_handle_t handle, int value)
{
    if (value != 0) {
        current_brightness = value;
    }
    return app_driver_light_refresh(handle);
}

esp_err_t app_driver_light_set_hue(app_driver_handle_t handle, int value)
{
    current_hue = value;
    current_temperature_mode = false;
    return app_driver_light_refresh(handle);
}

esp_err_t app_driver_light_set_saturation(app_driver_handle_t handle, int value)
{
    current_saturation = value;
    current_temperature_mode = false;
    return app_driver_light_refresh(handle);
}

esp_err_t app_driver_light_set_temperature(app_driver_handle_t handle, int value)
{
    constexpr int min_temperature = 2000;
    constexpr int max_temperature = 6500;
    constexpr int warm_red = 255;
    constexpr int warm_green = 180;
    constexpr int warm_blue = 80;
    constexpr int cool_red = 180;
    constexpr int cool_green = 220;
    constexpr int cool_blue = 255;

    int temperature = value < min_temperature ? min_temperature : value;
    temperature = temperature > max_temperature ? max_temperature : temperature;
    int position = temperature - min_temperature;
    int range = max_temperature - min_temperature;

    current_temperature_red = warm_red + ((cool_red - warm_red) * position) / range;
    current_temperature_green = warm_green + ((cool_green - warm_green) * position) / range;
    current_temperature_blue = warm_blue + ((cool_blue - warm_blue) * position) / range;
    current_temperature_mode = true;

    ESP_LOGI(TAG, "Color temperature: %d K, emulated as RGB: (%d, %d, %d)", value, current_temperature_red,
             current_temperature_green, current_temperature_blue);
    return app_driver_light_refresh(handle);
}

static void app_driver_button_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
    uint16_t endpoint_id = light_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, endpoint_id);
    cluster_t *cluster = cluster::get(endpoint, cluster_id);
    attribute_t *attribute = attribute::get(cluster, attribute_id);

    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(endpoint_id, cluster_id, attribute_id, &val);
}

esp_err_t app_driver_light_set_defaults()
{
    esp_err_t err = ESP_OK;
    uint16_t endpoint_id = light_endpoint_id;
    void *priv_data = endpoint::get_priv_data(endpoint_id);
    app_driver_handle_t handle = static_cast<app_driver_handle_t>(priv_data);
    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, endpoint_id);
    cluster_t *cluster = NULL;
    attribute_t *attribute = NULL;
    esp_matter_attr_val_t val = esp_matter_invalid(NULL);

    /* Setting brightness */
    cluster = cluster::get(endpoint, LevelControl::Id);
    attribute = attribute::get(cluster, LevelControl::Attributes::CurrentLevel::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_brightness(handle, REMAP_TO_RANGE(val.val.u8, MATTER_BRIGHTNESS, STANDARD_BRIGHTNESS));

    /* Setting color */
    cluster = cluster::get(endpoint, ColorControl::Id);
    attribute = attribute::get(cluster, ColorControl::Attributes::ColorMode::Id);
    attribute::get_val(attribute, &val);
    if (val.val.u8 == static_cast<uint8_t>(ColorControl::ColorMode::kCurrentHueAndCurrentSaturation)) {
        /* Setting hue */
        attribute = attribute::get(cluster, ColorControl::Attributes::CurrentHue::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_hue(handle, REMAP_TO_RANGE(val.val.u8, MATTER_HUE, STANDARD_HUE));
        /* Setting saturation */
        attribute = attribute::get(cluster, ColorControl::Attributes::CurrentSaturation::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_saturation(handle, REMAP_TO_RANGE(val.val.u8, MATTER_SATURATION, STANDARD_SATURATION));
    } else if (val.val.u8 == static_cast<uint8_t>(ColorControl::ColorMode::kColorTemperature)) {
        /* Setting temperature */
        attribute = attribute::get(cluster, ColorControl::Attributes::ColorTemperatureMireds::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_temperature(handle, REMAP_TO_RANGE_INVERSE(val.val.u16, STANDARD_TEMPERATURE_FACTOR));
    } else {
        ESP_LOGE(TAG, "Color mode not supported");
    }

    /* Setting power */
    cluster = cluster::get(endpoint, OnOff::Id);
    attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_power(handle, val.val.b);

    return err;
}

app_driver_handle_t app_driver_light_init()
{
    ESP_LOGI(TAG, "Initializing RGB light driver");
    return static_cast<app_driver_handle_t>(app_end_device_led_init());
}

app_driver_handle_t app_driver_button_init(void *user_data)
{
    button_handle_t handle = app_end_device_button_init();
    if (!handle) {
        return NULL;
    }

    iot_button_register_cb(handle, BUTTON_SINGLE_CLICK, NULL, app_driver_button_toggle_cb, user_data);
    return (app_driver_handle_t)handle;
}
