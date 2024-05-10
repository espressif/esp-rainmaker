/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/ledc.h>
#include <esp_err.h>

#include "ledc_driver.h"

/**
 *  @brief LEDC driver: Basic LEDC driver
 */

#define IS_ACTIVE_HIGH          0
#define LEDC_LS_TIMER           LEDC_TIMER_0
#define LEDC_LS_MODE            LEDC_LOW_SPEED_MODE

#define LEDC_LS_CH0_GPIO        (0)
#define LEDC_LS_CH0_CHANNEL     LEDC_CHANNEL_0
#define LEDC_LS_CH1_GPIO        (1)
#define LEDC_LS_CH1_CHANNEL     LEDC_CHANNEL_1
#define LEDC_LS_CH2_GPIO        (8)
#define LEDC_LS_CH2_CHANNEL     LEDC_CHANNEL_2

#define LEDC_NUM_CHANNELS       (3)
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY_MAX           (8192 - 1) // (2 ** 13) - 1
#define LEDC_FREQUENCY          (5000) // Frequency in Hertz. Set frequency at 5 kHz

/*
    * Prepare individual configuration
    * for each channel of LED Controller
    * by selecting:
    * - controller's channel number
    * - output duty cycle, set initially to 0
    * - GPIO number where LED is connected to
    * - speed mode, either high or low
    * - timer servicing selected channel
    *   Note: if different channels use one timer,
    *         then frequency and bit_num of these channels
    *         will be the same
    */
static ledc_channel_config_t ledc_channel[LEDC_NUM_CHANNELS] = {
    {
        .channel    = LEDC_LS_CH0_CHANNEL,
        .duty       = 0,
        .gpio_num   = LEDC_LS_CH0_GPIO,
        .speed_mode = LEDC_LS_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_LS_TIMER,
    },
    {
        .channel    = LEDC_LS_CH1_CHANNEL,
        .duty       = 0,
        .gpio_num   = LEDC_LS_CH1_GPIO,
        .speed_mode = LEDC_LS_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_LS_TIMER,    },
    {
        .channel    = LEDC_LS_CH2_CHANNEL,
        .duty       = 0,
        .gpio_num   = LEDC_LS_CH2_GPIO,
        .speed_mode = LEDC_LS_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_LS_TIMER,
    },
};

esp_err_t ledc_init(void)
{
    /*
     * Prepare and set configuration of timers
     * that will be used by LED Controller
     */
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_DUTY_RES,   // resolution of PWM duty
        .freq_hz = LEDC_FREQUENCY,          // frequency of PWM signal
        .speed_mode = LEDC_LS_MODE,         // timer mode
        .timer_num = LEDC_LS_TIMER,         // timer index
        .clk_cfg = LEDC_AUTO_CLK,           // Auto select the source clock
    };
    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);

    // Set LED Controller with previously prepared configuration
    for (int ch = 0; ch < LEDC_NUM_CHANNELS; ch++) {
        ledc_channel_config(&ledc_channel[ch]);
    }
    return ESP_OK;
}

static void ledc_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

esp_err_t ledc_set_rgb(uint32_t red, uint32_t green, uint32_t blue)
{
    red = red * LEDC_DUTY_MAX / 255;
    green = green * LEDC_DUTY_MAX / 255;
    blue = blue * LEDC_DUTY_MAX / 255;

    if (!IS_ACTIVE_HIGH) {
        red = LEDC_DUTY_MAX - red;
        green = LEDC_DUTY_MAX - green;
        blue = LEDC_DUTY_MAX - blue;
    }

    ledc_set_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel, red);
    ledc_update_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel);

    ledc_set_duty(ledc_channel[1].speed_mode, ledc_channel[1].channel, green);
    ledc_update_duty(ledc_channel[1].speed_mode, ledc_channel[1].channel);

    ledc_set_duty(ledc_channel[2].speed_mode, ledc_channel[2].channel, blue);
    ledc_update_duty(ledc_channel[2].speed_mode, ledc_channel[2].channel);
    return ESP_OK;
}

esp_err_t ledc_set_hsv(uint32_t hue, uint32_t saturation, uint32_t value)
{
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    ledc_hsv2rgb(hue, saturation, value, &red, &green, &blue);
    return ledc_set_rgb(red, green, blue);
}

esp_err_t ledc_clear()
{
    return ledc_set_rgb(0, 0, 0);
    return ESP_OK;
}
