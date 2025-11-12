/*  LED Lightbulb demo implementation using RGB LED

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <sdkconfig.h>
#include <esp_log.h>
#include <iot_button.h>
#include <button_gpio.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>

#include <app_reset.h>

#include <led_indicator.h>
#include <led_convert.h>
#ifdef CONFIG_LED_TYPE_RGB
#include <led_indicator_rgb.h>
#elif defined(CONFIG_LED_TYPE_WS2812)
#include <led_indicator_strips.h>
#endif

#include "app_priv.h"

/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10

static uint16_t g_hue = DEFAULT_HUE;
static uint16_t g_saturation = DEFAULT_SATURATION;
static uint16_t g_value = DEFAULT_BRIGHTNESS;
static bool g_power = DEFAULT_POWER;

/* LED Indicator handle */
static led_indicator_handle_t g_led_indicator = NULL;

esp_err_t app_light_set_led(uint32_t hue, uint32_t saturation, uint32_t brightness)
{
    /* Whenever this function is called, light power will be ON */
    if (!g_power) {
        g_power = true;
        esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_type(light_device, ESP_RMAKER_PARAM_POWER),
                esp_rmaker_bool(g_power));
    }

#ifdef CONFIG_LED_TYPE_NONE
    /* No LED hardware - just return success */
    return ESP_OK;
#endif

    if (!g_led_indicator) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Convert RainMaker format (H: 0-360, S: 0-100%, V: 0-100%) to led_indicator format (H: 0-359, S: 0-255, V: 0-255) */
    uint16_t hue_359 = hue % 360;  /* Wrap 360 to 0, handle any hue >= 360 */
    uint16_t sat_255 = (saturation * 255) / 100;
    uint16_t val_255 = (brightness * 255) / 100;
    /* Use MAX_INDEX to control all LEDs (works for both RGB and WS2812) */
    return led_indicator_set_hsv(g_led_indicator, SET_IHSV(MAX_INDEX, hue_359, sat_255, val_255));
}

esp_err_t app_light_set_power(bool power)
{
    g_power = power;
#ifdef CONFIG_LED_TYPE_NONE
    /* No LED hardware - just return success */
    return ESP_OK;
#endif

    if (!g_led_indicator) {
        return ESP_ERR_INVALID_STATE;
    }

    if (power) {
        uint16_t hue_359 = g_hue % 360;
        uint16_t sat_255 = (g_saturation * 255) / 100;
        uint16_t val_255 = (g_value * 255) / 100;
        /* Use MAX_INDEX to control all LEDs (works for both RGB and WS2812) */
        return led_indicator_set_hsv(g_led_indicator, SET_IHSV(MAX_INDEX, hue_359, sat_255, val_255));
    } else {
        return led_indicator_set_brightness(g_led_indicator, 0);
    }
}

esp_err_t app_light_init(void)
{
#ifdef CONFIG_LED_TYPE_RGB
    /* Use RGB LED (3-pin RGB LED) */
    led_indicator_rgb_config_t rgb_config = {
        .timer_inited = false,
        .timer_num = LEDC_TIMER_0,
        .red_gpio_num = CONFIG_RGB_LED_RED_GPIO,
        .green_gpio_num = CONFIG_RGB_LED_GREEN_GPIO,
        .blue_gpio_num = CONFIG_RGB_LED_BLUE_GPIO,
        .red_channel = LEDC_CHANNEL_0,
        .green_channel = LEDC_CHANNEL_1,
        .blue_channel = LEDC_CHANNEL_2,
    };
#ifdef CONFIG_RGB_LED_ACTIVE_LEVEL_HIGH
    rgb_config.is_active_level_high = true;
#else
    rgb_config.is_active_level_high = false;
#endif

    led_indicator_config_t config = {
        .blink_lists = NULL,
        .blink_list_num = 0,
    };

    esp_err_t err = led_indicator_new_rgb_device(&config, &rgb_config, &g_led_indicator);
#elif defined(CONFIG_LED_TYPE_WS2812)
    /* Use LED Strip (WS2812) */
    led_indicator_strips_config_t strips_config = {
        .led_strip_cfg = {
            .strip_gpio_num = CONFIG_WS2812_LED_GPIO,
            .max_leds = CONFIG_WS2812_LED_COUNT,
            .led_model = LED_MODEL_WS2812,
            .flags.invert_out = false,
        },
        .led_strip_driver = LED_STRIP_RMT,
        .led_strip_rmt_cfg = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = 10 * 1000 * 1000, /* 10 MHz */
            .flags.with_dma = false,
        },
    };

    led_indicator_config_t config = {
        .blink_lists = NULL,
        .blink_list_num = 0,
    };

    esp_err_t err = led_indicator_new_strips_device(&config, &strips_config, &g_led_indicator);
#else
    /* No LED hardware - g_led_indicator remains NULL */
    esp_err_t err = ESP_OK;
#endif

    if (err != ESP_OK) {
        return err;
    }

#ifdef CONFIG_LED_TYPE_NONE
    /* No LED hardware - just return success */
    return ESP_OK;
#endif

    if (g_power) {
        uint16_t hue_359 = g_hue % 360;
        uint16_t sat_255 = (g_saturation * 255) / 100;
        uint16_t val_255 = (g_value * 255) / 100;
        /* Use MAX_INDEX to control all LEDs (works for both RGB and WS2812) */
        return led_indicator_set_hsv(g_led_indicator, SET_IHSV(MAX_INDEX, hue_359, sat_255, val_255));
    } else {
        return led_indicator_set_brightness(g_led_indicator, 0);
    }
}

esp_err_t app_light_set_brightness(uint16_t brightness)
{
    g_value = brightness;
    return app_light_set_led(g_hue, g_saturation, g_value);
}
esp_err_t app_light_set_hue(uint16_t hue)
{
    g_hue = hue;
    return app_light_set_led(g_hue, g_saturation, g_value);
}
esp_err_t app_light_set_saturation(uint16_t saturation)
{
    g_saturation = saturation;
    return app_light_set_led(g_hue, g_saturation, g_value);
}

static void push_btn_cb(void *arg, void *data)
{
    app_light_set_power(!g_power);
    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_type(light_device, ESP_RMAKER_PARAM_POWER),
            esp_rmaker_bool(g_power));
}

void app_driver_init()
{
    app_light_init();
    button_config_t btn_cfg = {
        .long_press_time = 0,  /* Use default */
        .short_press_time = 0, /* Use default */
    };
    button_gpio_config_t gpio_cfg = {
        .gpio_num = BUTTON_GPIO,
        .active_level = BUTTON_ACTIVE_LEVEL,
        .enable_power_save = false,
    };
    button_handle_t btn_handle = NULL;
    if (iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn_handle) == ESP_OK && btn_handle) {
        /* Register a callback for a button single click event */
        iot_button_register_cb(btn_handle, BUTTON_SINGLE_CLICK, NULL, push_btn_cb, NULL);
        /* Register Wi-Fi reset and factory reset functionality on same button */
        app_reset_button_register(btn_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
    }
}
