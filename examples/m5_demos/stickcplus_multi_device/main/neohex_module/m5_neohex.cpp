/*!
 * @copyright This example code is in the Public Domain (or CC0 licensed, at your option.)

 * Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#include "m5_neohex.h"

CRGB leds[NUM_LEDS];
int smallArray[NUM_LEDS_LIGHT_DEVICE] = {5,  6,  7,  10, 11, 12, 13, 16, 17, 18,
                                         19, 20, 23, 24, 25, 26, 29, 30, 31};

// Neopixel initialization
void init_neohex(void) 
{
    FastLED.addLeds<WS2811, Neopixel_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
}

static void off_neo(void) 
{
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
    }
    FastLED.show();
    vTaskDelay(200 / portTICK_PERIOD_MS);
}

static uint8_t map_brightness(uint8_t brightness) 
{
    if (is_sgl_sw_scrn || lv_obj_has_state(light_sw, LV_STATE_CHECKED)) {
        return map(brightness, 0, 100, 0, 255);
    } else {
        return NULL_BRT;
    }
}

static uint8_t map_fastled_hue(uint16_t hue_value) 
{ 
    return (hue_value * 255) / 360; 
}

/**
 * This section is for single switch device control
 */
//=======================================================================================================================

static bool check_sgl_sw_pressed(void) 
{
    return is_sgl_sw_scrn && lv_obj_has_state(sgl_sw, LV_STATE_CHECKED);
}

static void on_sgl_sw_neo(void) 
{
    uint8_t brt = map_brightness(DEFAULT_BRT);
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(PURPLE_COLOR, DEFAULT_SAT, brt);
    }
    FastLED.show();
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

/**
 * This section is for light device control
 */
//=======================================================================================================================

static bool check_light_sw_pressed(void) 
{
    return is_light_scrn && lv_obj_has_state(light_sw, LV_STATE_CHECKED);
}

static void on_light_sw_neo(void) 
{
    uint8_t brt = map_brightness(brt_level);
    uint8_t hue = map_fastled_hue(hue_level);
    for (int i = 0; i < NUM_LEDS_LIGHT_DEVICE; i++) {
        leds[smallArray[i]] = CHSV(hue, DEFAULT_SAT, brt);
    }
    FastLED.show();
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

void set_brt(void) 
{
    uint8_t brt;
    uint8_t hue;
    brt = map_brightness(brt_level);
    hue = map_fastled_hue(hue_level);
    for (int i = 0; i < NUM_LEDS_LIGHT_DEVICE; i++) {
        leds[smallArray[i]] = CHSV(hue, 255, brt);
    }
    FastLED.show();
}

void set_hue(void) 
{
    uint8_t brt;
    uint8_t hue;
    brt = map_brightness(brt_level);
    hue = map_fastled_hue(hue_level);
    for (int i = 0; i < NUM_LEDS_LIGHT_DEVICE; i++) {
        leds[smallArray[i]] = CHSV(hue, 255, brt);
    }
    FastLED.show();
}

/**
 * This section is for main set lighting control
 */
//=======================================================================================================================

void set_lighting_state(bool state) 
{
    if (check_sgl_sw_pressed() && state) {
        on_sgl_sw_neo();
    } else if (check_light_sw_pressed() && state) {
        on_light_sw_neo();
    } else {
        off_neo();
    }
}