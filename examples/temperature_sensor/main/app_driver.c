/*  Temperature Sensor demo implementation using RGB LED and timer

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <sdkconfig.h>
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>

#include <app_reset.h>

#include <led_indicator.h>
#include <led_convert.h>
#if CONFIG_IDF_TARGET_ESP32C2
#include <led_indicator_rgb.h>
#else
#include <led_indicator_strips.h>
#endif

#include "app_priv.h"

/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0
/* This is the GPIO on which the power will be set */
#define OUTPUT_GPIO    19

static TimerHandle_t sensor_timer;

#define DEFAULT_SATURATION  100
#define DEFAULT_BRIGHTNESS  50

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10

static float g_temperature;

static uint16_t g_hue;
static uint16_t g_saturation = DEFAULT_SATURATION;
static uint16_t g_value = DEFAULT_BRIGHTNESS;

/* LED Indicator handle */
static led_indicator_handle_t g_led_indicator = NULL;

static esp_err_t app_temp_set_led(uint32_t hue, uint32_t saturation, uint32_t brightness)
{
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
    return led_indicator_set_hsv(g_led_indicator, SET_IHSV(MAX_INDEX, hue_359, sat_255, val_255));
}

static void app_sensor_update(TimerHandle_t handle)
{
    static float delta = 0.5;
    g_temperature += delta;
    if (g_temperature > 99) {
        delta = -0.5;
    } else if (g_temperature < 1) {
        delta = 0.5;
    }
    g_hue = (100 - g_temperature) * 2;
    app_temp_set_led(g_hue, g_saturation, g_value);
    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_type(temp_sensor_device, ESP_RMAKER_PARAM_TEMPERATURE),
            esp_rmaker_float(g_temperature));
}

float app_get_current_temperature()
{
    return g_temperature;
}

esp_err_t app_sensor_init(void)
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

    g_temperature = DEFAULT_TEMPERATURE;
    sensor_timer = xTimerCreate("app_sensor_update_tm", (REPORTING_PERIOD * 1000) / portTICK_PERIOD_MS,
                            pdTRUE, NULL, app_sensor_update);
    if (sensor_timer) {
        xTimerStart(sensor_timer, 0);
        g_hue = (100 - g_temperature) * 2;
#ifndef CONFIG_LED_TYPE_NONE
        app_temp_set_led(g_hue, g_saturation, g_value);
#endif
        return ESP_OK;
    }
    return ESP_FAIL;
}

void app_driver_init()
{
    app_sensor_init();
    app_reset_button_register(app_reset_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL),
                WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
}
