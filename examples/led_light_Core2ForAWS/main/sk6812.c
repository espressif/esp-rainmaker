/*
 * This file is part of the MicroPython ESP32 project, https://github.com/loboris/MicroPython_ESP32_psRAM_LoBo
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 LoBo (https://github.com/loboris)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "driver/rmt.h"
#include "esp_log.h"

#include "sk6812.h"

#include "soc/dport_access.h"
#include "soc/dport_reg.h"

static SemaphoreHandle_t neopixel_sem = NULL;
static uint16_t neopixel_buf_len = 0;
static uint8_t *neopixel_buffer = NULL;
static uint32_t neopixel_t0h_ticks = 0;
static uint32_t neopixel_t1h_ticks = 0;
static uint32_t neopixel_t0l_ticks = 0;
static uint32_t neopixel_t1l_ticks = 0;
static uint32_t neopixel_reset_ticks = 0;

// Get color value of RGB component
//---------------------------------------------------
static uint8_t offset_color(char o, uint32_t color) {
	uint8_t clr = 0;
	switch(o) {
		case 'R':
			clr = (uint8_t)(color >> 24);
			break;
		case 'G':
			clr = (uint8_t)(color >> 16);
			break;
		case 'B':
			clr = (uint8_t)(color >> 8);
			break;
		case 'W':
			clr = (uint8_t)(color & 0xFF);
			break;
		default:
			clr = 0;
	}
	return clr;
}

// Set pixel color at buffer position from RGB color value
//=========================================================================
void np_set_pixel_color(pixel_settings_t *px, uint16_t idx, uint32_t color) {
	uint16_t ofs = idx * (px->nbits / 8);
	px->pixels[ofs] = offset_color(px->color_order[0], color);
	px->pixels[ofs+1] = offset_color(px->color_order[1], color);
	px->pixels[ofs+2] = offset_color(px->color_order[2], color);
	if (px->nbits == 32) px->pixels[ofs+3] = offset_color(px->color_order[3], color);
}

// Set pixel color at buffer position from HSB color value
//============================================================================================================
void np_set_pixel_color_hsb(pixel_settings_t *px, uint16_t idx, float hue, float saturation, float brightness)
{
	uint32_t color = hsb_to_rgb(hue, saturation, brightness);
	np_set_pixel_color(px, idx, color);
}

// Get RGB color value from RGB components corrected by brightness factor
//=============================================================================
uint32_t np_get_pixel_color(pixel_settings_t *px, uint16_t idx, uint8_t *white)
{
	uint32_t clr = 0;
	uint32_t color = 0;
	uint8_t bpp = px->nbits/8;
	uint16_t ofs = idx * bpp;

	for (int i=0; i < bpp; i++) {
		clr = (uint16_t)px->pixels[ofs+i];
		switch(px->color_order[i]) {
			case 'R':
				color |= (uint32_t)clr << 16;
				break;
			case 'G':
				color |= (uint32_t)clr << 8;
				break;
			case 'B':
				color |= (uint32_t)clr;
				break;
			case 'W':
				*white = px->pixels[ofs+i];
				break;
		}
	}
	return color;
}

static void IRAM_ATTR np_rmt_adapter(const void *src, rmt_item32_t *dest, size_t src_size,
        size_t wanted_num, size_t *translated_size, size_t *item_num) {
    if (src == NULL || dest == NULL) {
        *translated_size = 0;
        *item_num = 0;
        return;
    }
    const rmt_item32_t bit0 = {{{ neopixel_t0h_ticks, 1, neopixel_t0l_ticks, 0 }}}; //Logical 0
    const rmt_item32_t bit1 = {{{ neopixel_t1h_ticks, 1, neopixel_t1l_ticks, 0 }}}; //Logical 1
    size_t size = 0;
    size_t num = 0;
    uint8_t *psrc = (uint8_t *)src;
    rmt_item32_t *pdest = dest;
	uint8_t transmit_end = 0;	

	if ((wanted_num >> 3) >= src_size) {
		src_size -= 1;
		transmit_end = 1;
	}

    while (size < src_size && num < wanted_num) {
        for (int i = 0; i < 8; i++) {
            // MSB first
            if (*psrc & (1 << (7 - i))) {
                pdest->val =  bit1.val;
            } else {
                pdest->val =  bit0.val;
            }
            num++;
            pdest++;
        }
        size++;
        psrc++;
    }

	if (transmit_end) {
		const rmt_item32_t reset = {{{ neopixel_reset_ticks >> 1, 0, neopixel_reset_ticks >> 1, 0 }}};
		pdest->val = reset.val;
		size += 1;
		num += 1;
	}

	*translated_size = size;
    *item_num = num;
}

// Initialize Neopixel RMT interface on specific GPIO
//===================================================
int neopixel_init(int gpioNum, rmt_channel_t channel) {
	if (neopixel_sem == NULL) {
		neopixel_sem = xSemaphoreCreateBinary();
		if (neopixel_sem == NULL) return ESP_FAIL;
		xSemaphoreGive(neopixel_sem);
	}

	xSemaphoreTake(neopixel_sem, portMAX_DELAY);
    rmt_config_t config = {                                                
        .rmt_mode = RMT_MODE_TX,
        .channel = channel,
        .gpio_num = gpioNum,
        .clk_div = 2,
        .mem_block_num = 1, 
        .tx_config = {
            .idle_level = RMT_IDLE_LEVEL_LOW,
            .carrier_en = false,
            .loop_en = false,
            .idle_output_en = true,
        }                                            
    };

	esp_err_t res = ESP_OK;
	res = rmt_config(&config);
	if (res != ESP_OK) {
		goto failed;
	}

	res = rmt_driver_install(config.channel, 0, 0);
	if (res != ESP_OK) {
		goto failed;
	}

    res = rmt_translator_init(channel, np_rmt_adapter);
	if (res != ESP_OK) {
		goto failed;
	}

failed:
	xSemaphoreGive(neopixel_sem);
	return res;
}

// Deinitialize RMT interface
//=========================================
void neopixel_deinit(rmt_channel_t channel) {
	xSemaphoreTake(neopixel_sem, portMAX_DELAY);
	rmt_driver_uninstall(channel);
	xSemaphoreGive(neopixel_sem);
}

// Start the transfer of Neopixel color bytes from buffer
//=======================================================
void np_show(pixel_settings_t *px, rmt_channel_t channel)
{
	float ratio = 40000000.0 / 1e9; 
	uint16_t blen = px->pixel_count * (px->nbits / 8) + 1;

	xSemaphoreTake(neopixel_sem, portMAX_DELAY);
	neopixel_t0h_ticks = (uint32_t)(ratio * px->timings.t0h);
    neopixel_t0l_ticks = (uint32_t)(ratio * px->timings.t0l);
    neopixel_t1h_ticks = (uint32_t)(ratio * px->timings.t1h);
    neopixel_t1l_ticks = (uint32_t)(ratio * px->timings.t1l);
	neopixel_reset_ticks = (uint32_t)(ratio * px->timings.reset);

	// Allocate neopixel buffer if needed
	if (neopixel_buffer == NULL) {
		neopixel_buffer = (uint8_t *)malloc(blen);
		if (neopixel_buffer == NULL) return;
		neopixel_buf_len = blen;
	}
	// Resize neopixel buffer if needed
	if (neopixel_buf_len < blen) {
		// larger buffer needed
		free(neopixel_buffer);
		neopixel_buffer = (uint8_t *)malloc(blen);
		if (neopixel_buffer == NULL) return;
	}

	memcpy(neopixel_buffer, px->pixels, blen);
	for (uint16_t i = 0; i < blen; i++) {
		float b = px->brightness / 255.0;
		neopixel_buffer[i] = b * neopixel_buffer[i]; 
	}
	rmt_write_sample(channel, neopixel_buffer, blen, true);
	xSemaphoreGive(neopixel_sem);
}

// Clear the Neopixel color buffer
//=================================
void np_clear(pixel_settings_t *px)
{
	memset(px->pixels, 0, px->pixel_count * (px->nbits/8));
}

//------------------------------------
static float Min(double a, double b) {
	return a <= b ? a : b;
}

//------------------------------------
static float Max(double a, double b) {
	return a >= b ? a : b;
}

// Convert 24-bit color to HSB representation
//===================================================================
void rgb_to_hsb( uint32_t color, float *hue, float *sat, float *bri )
{
	float delta, min;
	float h = 0, s, v;
	uint8_t red = (color >> 16) & 0xFF;
	uint8_t green = (color >> 8) & 0xFF;
	uint8_t blue = color & 0xFF;

	min = Min(Min(red, green), blue);
	v = Max(Max(red, green), blue);
	delta = v - min;

	if (v == 0.0) s = 0;
	else s = delta / v;

	if (s == 0)	h = 0.0;
	else
	{
		if (red == v)
			h = (green - blue) / delta;
		else if (green == v)
			h = 2 + (blue - red) / delta;
		else if (blue == v)
			h = 4 + (red - green) / delta;

		h *= 60;

		if (h < 0.0) h = h + 360;
	}

	*hue = h;
	*sat = s;
	*bri = v / 255;
}

// Convert HSB color to 24-bit color representation
//============================================================
uint32_t hsb_to_rgb(float _hue, float _sat, float _brightness)
{
	float red = 0.0;
	float green = 0.0;
	float blue = 0.0;

	if (_sat == 0.0) {
		red = _brightness;
		green = _brightness;
		blue = _brightness;
	}
	else {
		if (_hue >= 360.0) _hue = fmod(_hue, 360);

		int slice = (int)(_hue / 60.0);
		float hue_frac = (_hue / 60.0) - slice;

		float aa = _brightness * (1.0 - _sat);
		float bb = _brightness * (1.0 - _sat * hue_frac);
		float cc = _brightness * (1.0 - _sat * (1.0 - hue_frac));

		switch(slice) {
			case 0:
				red = _brightness;
				green = cc;
				blue = aa;
				break;
			case 1:
				red = bb;
				green = _brightness;
				blue = aa;
				break;
			case 2:
				red = aa;
				green = _brightness;
				blue = cc;
				break;
			case 3:
				red = aa;
				green = bb;
				blue = _brightness;
				break;
			case 4:
				red = cc;
				green = aa;
				blue = _brightness;
				break;
			case 5:
				red = _brightness;
				green = aa;
				blue = bb;
				break;
			default:
				red = 0.0;
				green = 0.0;
				blue = 0.0;
				break;
		}
	}

	return (uint32_t)((uint8_t)(red * 255.0) << 16) | ((uint8_t)(green * 255.0) << 8) | ((uint8_t)(blue * 255.0));
}

// Convert HSB color to 24-bit color representation
// _hue: 0 ~ 359
// _sat: 0 ~ 255
// _bri: 0 ~ 255
//=======================================================
uint32_t hsb_to_rgb_int(int hue, int sat, int brightness)
{
	float _hue = (float)hue;
	float _sat = (float)((float)sat / 1000.0);
	float _brightness = (float)((float)brightness / 1000.0);
	float red = 0.0;
	float green = 0.0;
	float blue = 0.0;

	if (_sat == 0.0) {
		red = _brightness;
		green = _brightness;
		blue = _brightness;
	}
	else {
		if (_hue >= 360.0) _hue = fmod(_hue, 360);

		int slice = (int)(_hue / 60.0);
		float hue_frac = (_hue / 60.0) - slice;

		float aa = _brightness * (1.0 - _sat);
		float bb = _brightness * (1.0 - _sat * hue_frac);
		float cc = _brightness * (1.0 - _sat * (1.0 - hue_frac));

		switch(slice) {
			case 0:
				red = _brightness;
				green = cc;
				blue = aa;
				break;
			case 1:
				red = bb;
				green = _brightness;
				blue = aa;
				break;
			case 2:
				red = aa;
				green = _brightness;
				blue = cc;
				break;
			case 3:
				red = aa;
				green = bb;
				blue = _brightness;
				break;
			case 4:
				red = cc;
				green = aa;
				blue = _brightness;
				break;
			case 5:
				red = _brightness;
				green = aa;
				blue = bb;
				break;
			default:
				red = 0.0;
				green = 0.0;
				blue = 0.0;
				break;
		}
	}

	return (uint32_t)((uint8_t)(red * 255.0) << 16) | ((uint8_t)(green * 255.0) << 8) | ((uint8_t)(blue * 255.0));
}
