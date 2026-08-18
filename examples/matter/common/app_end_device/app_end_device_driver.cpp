/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <led_convert.h>
#include <led_indicator.h>
#include <led_indicator_gpio.h>
#include <led_indicator_rgb.h>
#include <led_indicator_strips.h>
#include <sdkconfig.h>
#include <soc/soc_caps.h>

#include <app_end_device.h>

static const char *TAG = "app_end_device_driver";

led_indicator_handle_t app_end_device_led_init(void)
{
    led_indicator_handle_t handle = NULL;
#if !CONFIG_APP_END_DEVICE_LED_TYPE_NONE
    const led_indicator_config_t config = {};
#endif

#if CONFIG_APP_END_DEVICE_LED_TYPE_GPIO
    const led_indicator_gpio_config_t gpio_config = {
        .is_active_level_high = true,
        .gpio_num = CONFIG_APP_END_DEVICE_LED_GPIO,
    };
    esp_err_t err = led_indicator_new_gpio_device(&config, &gpio_config, &handle);
#elif CONFIG_APP_END_DEVICE_LED_TYPE_RGB
    const led_indicator_rgb_config_t rgb_config = {
        .is_active_level_high = false,
        .timer_inited = false,
        .timer_num = LEDC_TIMER_0,
        .red_gpio_num = GPIO_NUM_0,
        .green_gpio_num = GPIO_NUM_1,
        .blue_gpio_num = GPIO_NUM_8,
        .red_channel = LEDC_CHANNEL_0,
        .green_channel = LEDC_CHANNEL_1,
        .blue_channel = LEDC_CHANNEL_2,
    };
    esp_err_t err = led_indicator_new_rgb_device(&config, &rgb_config, &handle);
#elif CONFIG_APP_END_DEVICE_LED_TYPE_WS2812
    const led_indicator_strips_config_t strips_config = {
        .led_strip_cfg = {
            .strip_gpio_num = CONFIG_APP_END_DEVICE_WS2812_LED_GPIO,
            .max_leds = 1,
            .led_model = LED_MODEL_WS2812,
            .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
            .flags = {
                .invert_out = false,
            },
        },
#if SOC_RMT_SUPPORTED
        .led_strip_driver = LED_STRIP_RMT,
        .led_strip_rmt_cfg = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = 10 * 1000 * 1000,
            .mem_block_symbols = 64,
            .flags = {
                .with_dma = false,
            },
        },
#elif SOC_GPSPI_SUPPORTED
        .led_strip_driver = LED_STRIP_SPI,
        .led_strip_spi_cfg = {
            .clk_src = SPI_CLK_SRC_DEFAULT,
            .spi_bus = SPI2_HOST,
            .flags = {
                .with_dma = false,
            },
        },
#else
#error "No supported LED strip backend"
#endif
    };
    esp_err_t err = led_indicator_new_strips_device(&config, &strips_config, &handle);
#else
    esp_err_t err = ESP_OK;
#endif

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LED: %s", esp_err_to_name(err));
        return NULL;
    }
    return handle;
}

esp_err_t app_end_device_led_set_rgb(led_indicator_handle_t handle, uint8_t red, uint8_t green, uint8_t blue)
{
#if CONFIG_APP_END_DEVICE_LED_TYPE_NONE
    return ESP_OK;
#else
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_APP_END_DEVICE_LED_TYPE_GPIO
    bool on = red || green || blue;
    return led_indicator_set_on_off(handle, on);
#else
    return led_indicator_set_rgb(handle, SET_IRGB(0, red, green, blue));
#endif
#endif
}

button_handle_t app_end_device_button_init(void)
{
    const button_config_t button_config = {};
    const button_gpio_config_t gpio_config = {
        .gpio_num = CONFIG_APP_END_DEVICE_BOARD_BUTTON_GPIO,
        .active_level = 0,
    };
    button_handle_t handle = NULL;
    esp_err_t err = iot_button_new_gpio_device(&button_config, &gpio_config, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize button: %s", esp_err_to_name(err));
        return NULL;
    }
    return handle;
}
