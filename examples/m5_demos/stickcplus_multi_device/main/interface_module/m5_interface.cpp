/*
* SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
* 
* SPDX-License-Identifier: Apache-2.0
*/
#include "m5_interface.h"

/* Provision screen interface */
static lv_obj_t *qr = NULL;
static lv_obj_t *pop_code = NULL;

/* Start screen interface */
lv_obj_t *start_scrn_bg = NULL;
lv_obj_t *start_btn = NULL;
lv_obj_t *start_header = NULL;
lv_style_t start_btn_style;

/* Single Slider on/off switch */
lv_obj_t *sgl_sw_scrn_bg = NULL;
lv_obj_t *sgl_sw = NULL;
lv_obj_t *sgl_sw_header = NULL;

/* Light device interface */
lv_obj_t *light_scrn_bg = NULL;
lv_obj_t *light_sw = NULL;
lv_obj_t *light_header = NULL;
lv_obj_t *brt_slider = NULL;
lv_obj_t *hue_slider = NULL;

/* Relay device interface */
lv_obj_t *relay_scrn_bg = NULL;
lv_obj_t *relay_header = NULL;
lv_obj_t *relay_sw_1 = NULL;
lv_obj_t *relay_sw_2 = NULL;
lv_obj_t *relay_sw_3 = NULL;
lv_obj_t *relay_sw_4 = NULL;

/* Relay device interface */
lv_obj_t *wifi_rst_block = NULL;
lv_obj_t *fctry_rst_block = NULL;

/* Screen state */
bool is_start_scrn = false;
bool is_sgl_sw_scrn = false;
bool is_light_scrn = false;
bool is_relay_scrn = false;
bool is_wifi_reset = false;
bool is_factory_reset = false;

static const char *TAG = "app_reset";

/**
 * This section is for qr code generator.
 */
//=======================================================================================================================

/*!
 *  @brief Delete the qr code after network success connection.
 */
void del_prov_display(void) 
{
    if (lvgl_port_lock()) {
        if (qr != NULL) {
            lv_obj_del(qr);
            qr = NULL;
        }
        if (pop_code != NULL) {
            lv_obj_del(pop_code);
            pop_code = NULL;
        }
        lvgl_port_unlock();
    }
}

/*!
 *  @brief Display the qrcode on m5 interface.
 *  @param payload This char that is taken from app_network.c to obtain the provision json.
 */
void display_qrcode_m5(const char *payload)
{
    if (lvgl_port_lock()) {
        lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5);
        qr = lv_qrcode_create(lv_scr_act());
        lv_qrcode_set_size(qr, 100);
        lv_obj_align(qr, LV_ALIGN_CENTER, 0, -15);

        const char *data = payload; // Set data
        Serial.println(payload);
        lv_qrcode_update(qr, data, strlen(data));

        lv_obj_set_style_border_color(qr, bg_color, 0);
        lv_obj_set_style_border_width(qr, 5, 0);
        lvgl_port_unlock();
    }
}

/*!
 *  @brief Display the pop code on m5 interface.
 *  @param payload This char that is taken from app_network.c to obtain the provision json.
 */
void display_pop_m5(const char *pop)
{
    if (lvgl_port_lock()) {
        pop_code = lv_btn_create(lv_scr_act());
        lv_obj_align(pop_code, LV_ALIGN_BOTTOM_MID, 0, -26);

        lv_obj_t *pop_label = lv_label_create(pop_code);
        char buf[20]; 
        snprintf(buf, sizeof(buf), "Pop Code: %s", pop); 
        lv_label_set_text(pop_label, buf);
        lv_obj_set_style_text_font(pop_label, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_align(pop_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(pop_label, 100);
        lv_label_set_long_mode(pop_label, LV_LABEL_LONG_WRAP);
        
        lvgl_port_unlock();
    }
}

/**
 * This section is for M5 initialisation & screen flags.
 */
//=======================================================================================================================

void init_m5(void) 
{
    M5.begin();
    lvgl_port_init(M5.Lcd);
    M5.Power.begin();
}

void set_start_scrn(void) 
{
    is_start_scrn = true;
    is_sgl_sw_scrn = false;
    is_light_scrn = false;
    is_relay_scrn = false;
}

void set_sgle_sw_scrn(void) 
{
    is_start_scrn = false;
    is_sgl_sw_scrn = true;
    is_light_scrn = false;
    is_relay_scrn = false;
}

void set_light_scrn(void) 
{
    is_start_scrn = false;
    is_sgl_sw_scrn = false;
    is_light_scrn = true;
    is_relay_scrn = false;
}

void set_relay_sw_scrn(void) 
{
    is_start_scrn = false;
    is_sgl_sw_scrn = false;
    is_light_scrn = false;
    is_relay_scrn = true;
}

void set_wifi_reset_notice(void) 
{
    is_wifi_reset = true;
    is_factory_reset = false;
    display_wifi_rst();
}

void set_fctry_reset_notice(void) 
{
    is_factory_reset = true;
    display_fctry_rst();
}

/**
 * This section is for reset notice interface
 */
//=======================================================================================================================

static void create_block(lv_obj_t **block) 
{
    *block = lv_btn_create(lv_scr_act());
    lv_obj_set_size(*block, 115, 50);
    lv_obj_align(*block, LV_ALIGN_CENTER, 0, 0);
}

static void rst_label(lv_obj_t *block, const char *text) 
{
    lv_obj_t *rst_label = lv_label_create(block);
    lv_label_set_text(rst_label, text);
    lv_obj_set_style_text_color(rst_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(rst_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(rst_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_align(rst_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(rst_label, 100);
    lv_label_set_long_mode(rst_label, LV_LABEL_LONG_WRAP);
}

void display_wifi_rst(void) 
{
    ESP_LOGI(TAG, "Release button now for Wi-Fi reset. Keep pressed for factory reset.");

    if (wifi_rst_block == NULL) {
        if (lvgl_port_lock()) {
            create_block(&wifi_rst_block);
            static lv_style_t style_wifi_rst;
            lv_style_init(&style_wifi_rst);
            lv_style_set_bg_color(&style_wifi_rst, lv_color_hex(COLOR_GREY));
            lv_style_set_border_color(&style_wifi_rst, lv_color_hex(COLOR_RED));
            lv_style_set_border_width(&style_wifi_rst, 2); // Set border width (e.g., 2px)
            lv_obj_add_style(wifi_rst_block, &style_wifi_rst, 0);
            rst_label(wifi_rst_block, "Release btn for WiFi reset");
            lvgl_port_unlock();
        }
    }
}

void display_fctry_rst(void) 
{
    ESP_LOGI(TAG, "Release button to trigger factory reset.");
    if (fctry_rst_block == NULL) {
        if (lvgl_port_lock()) {
            create_block(&fctry_rst_block);
            static lv_style_t style_fctry_rst;
            lv_style_init(&style_fctry_rst);
            lv_style_set_bg_color(&style_fctry_rst, lv_color_hex(COLOR_ORANGE));
            lv_style_set_border_color(&style_fctry_rst, lv_color_hex(COLOR_RED));
            lv_style_set_border_width(&style_fctry_rst, 2); // Set border width (e.g., 2px)
            lv_obj_add_style(fctry_rst_block, &style_fctry_rst, 0);
            rst_label(fctry_rst_block, "Release btn to Fctry reset");
            lvgl_port_unlock();
        }
    }
}

/**
 * This section is for start screen interface
 */
//=======================================================================================================================

void show_start_screen(void) 
{
    lv_scr_load_anim(start_scrn_bg, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, false);
    lv_style_reset(&start_btn_style);
}

void reset_start_btn(void) 
{
    if (lvgl_port_lock()) {
        lv_style_reset(&start_btn_style);
        lvgl_port_unlock();
    }
}

void display_start_btn_pressed(void) 
{
    if (lvgl_port_lock()) {
        lv_style_init(&start_btn_style);
        lv_style_set_bg_color(&start_btn_style,
                              lv_color_hex(COLOR_ORANGE));   // Set background color (e.g., orange)
        lv_style_set_bg_opa(&start_btn_style, LV_OPA_COVER); // Set the opacity to fully visible
        lv_style_set_radius(&start_btn_style, 10);           // Set corner radius for rounded edges
        lv_style_set_border_width(&start_btn_style, 2);      // Set the border width
        lv_style_set_border_color(&start_btn_style,
                                  lv_color_hex(COLOR_CRIMSON)); // Set border color (e.g., red)

        lv_obj_add_style(start_btn, &start_btn_style, LV_PART_MAIN);
        lvgl_port_unlock();
    }
}

static void display_start_bg(void) 
{
    if (lvgl_port_lock()) {
        start_scrn_bg = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(start_scrn_bg, lv_color_hex(COLOR_SKY_BLUE), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(start_scrn_bg, LV_OPA_COVER, LV_PART_MAIN);
        lv_scr_load(start_scrn_bg);
        lvgl_port_unlock();
    }
}

static void display_start_btn(void) 
{
    if (lvgl_port_lock()) {
        start_btn = lv_btn_create(start_scrn_bg);
        lv_obj_align(start_btn, LV_ALIGN_BOTTOM_MID, 0, -30);

        lv_obj_t *btn_label = lv_label_create(start_btn);
        lv_label_set_text(btn_label, "Press to start");
        lv_obj_center(btn_label);

        lv_obj_add_event_cb(start_btn, start_btn_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(start_btn, start_btn_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(start_btn, start_btn_cb, LV_EVENT_REFRESH, NULL);

        lvgl_port_unlock();
    }
}

static void display_start_header(void) 
{
    start_header = lv_label_create(start_scrn_bg);
    lv_label_set_text(start_header, "ESP RainMaker");
    lv_obj_align(start_header, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(start_header, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_add_event_cb(start_header, scrn_event_cb, LV_EVENT_LONG_PRESSED, NULL);
}

void display_start_screen(void)
{
    set_start_scrn();
    display_start_bg();
    display_start_header();
    display_start_btn();
    display_rainmaker_icon();
}

/**
 * This section is for single switch device interface
 */
//=======================================================================================================================

static void display_sgl_sw_bg(void) 
{
    sgl_sw_scrn_bg = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(sgl_sw_scrn_bg, lv_color_hex(0xD3A6FF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sgl_sw_scrn_bg, LV_OPA_COVER, LV_PART_MAIN);
}

static void display_sgl_switch(void) 
{
    sgl_sw = lv_switch_create(sgl_sw_scrn_bg);
    lv_obj_align(sgl_sw, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_width(sgl_sw, lv_pct(70));

    lv_obj_add_event_cb(sgl_sw, my_single_switch_cb, LV_EVENT_CLICKED, NULL);
}

static void display_sgl_sw_header(void) 
{
    sgl_sw_header = lv_label_create(sgl_sw_scrn_bg);
    lv_label_set_text(sgl_sw_header, "Single Switch demo");
    lv_obj_align(sgl_sw_header, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(sgl_sw_header, &lv_font_montserrat_12, LV_PART_MAIN);

    lv_obj_add_event_cb(sgl_sw_header, scrn_event_cb, LV_EVENT_ALL, NULL);
}

void show_sgl_switch_screen(void) 
{
    if (lvgl_port_lock()) {
        lv_scr_load_anim(sgl_sw_scrn_bg, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, false);
        lvgl_port_unlock();
    }
    set_sgl_sw_state(rm_sgl_sw_state);
}

void display_sgl_switch_screen(void) 
{
    if (lvgl_port_lock()) {
        display_sgl_sw_bg();
        display_sgl_switch();
        display_sgl_sw_header();
        display_light_bulb();
        lvgl_port_unlock();
    }
}

/**
 * This section is for light device interface
 */
//=======================================================================================================================

static void display_light_bg(void) 
{
    light_scrn_bg = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(light_scrn_bg, lv_color_hex(0xABE9AB), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(light_scrn_bg, LV_OPA_COVER, LV_PART_MAIN);
}

static void display_light_sw(void) 
{
    light_sw = lv_switch_create(light_scrn_bg);
    lv_obj_set_size(light_sw, 60, 30);
    lv_obj_align(light_sw, LV_ALIGN_BOTTOM_MID, 0, -25);

    lv_obj_set_style_bg_color(light_sw, lv_color_hex(COLOR_ORANGE), LV_PART_KNOB); // Knob color
    lv_obj_set_style_radius(light_sw, LV_RADIUS_CIRCLE, LV_PART_KNOB);             // Knob shape

    static lv_obj_t *header = lv_label_create(light_scrn_bg);
    lv_label_set_text(header, "OFF/ON");
    lv_obj_align_to(header, light_sw, LV_ALIGN_BOTTOM_MID, 0, 20);

    lv_obj_add_event_cb(light_sw, light_sw_cb, LV_EVENT_CLICKED, NULL);
}

static void display_light_header(void) 
{
    light_header = lv_label_create(light_scrn_bg);
    lv_label_set_text(light_header, "Light device example");
    lv_obj_align(light_header, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(light_header, &lv_font_montserrat_12, LV_PART_MAIN);

    lv_obj_add_event_cb(light_header, scrn_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(light_header, light_sw_select_cb, LV_EVENT_SHORT_CLICKED, NULL);
}

static void display_light_brt(void) 
{
    brt_slider = lv_slider_create(light_scrn_bg);
    lv_slider_set_range(brt_slider, 0, 100);
    lv_slider_set_value(brt_slider, DEFAULT_BRT, LV_ANIM_ON);  // Set initial value to 0%
    lv_obj_set_size(brt_slider, 15, 100);                      // Adjust width and height for vertical slider
    lv_obj_set_style_base_dir(brt_slider, LV_BASE_DIR_LTR, 0); // Ensure layout aligns properly
    lv_obj_align(brt_slider, LV_ALIGN_CENTER, 30, -20);        // Position to the right of the switch

    lv_obj_set_style_bg_color(brt_slider, lv_color_hex(COLOR_WHITE), LV_PART_KNOB); // Knob color
    lv_obj_set_style_radius(brt_slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);            // Knob shape

    static lv_obj_t *header = lv_label_create(light_scrn_bg);
    lv_label_set_text(header, "BRT");
    lv_obj_align_to(header, brt_slider, LV_ALIGN_BOTTOM_MID, 0, 30);

    lv_obj_add_event_cb(brt_slider, light_brt_cb, LV_EVENT_ALL, NULL);
}

static void display_light_hue(void) 
{
    hue_slider = lv_slider_create(light_scrn_bg);
    lv_slider_set_range(hue_slider, 0, 360);                   // Set hue range (0 to 360 degrees)
    lv_obj_set_size(hue_slider, 15, 100);                      // Adjust width and height for vertical slider
    lv_obj_set_style_base_dir(hue_slider, LV_BASE_DIR_LTR, 0); // Ensure layout aligns properly
    lv_obj_align(hue_slider, LV_ALIGN_CENTER, -30, -20);
    lv_obj_set_style_bg_color(hue_slider, lv_color_hex(COLOR_WHITE), LV_PART_KNOB); // Knob color
    lv_obj_set_style_radius(hue_slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);            // Knob shape
    lv_slider_set_value(hue_slider, DEFAULT_HUE, LV_ANIM_ON); // Set initial value to 120 for green

    static lv_obj_t *header = lv_label_create(light_scrn_bg);
    lv_label_set_text(header, "HUE");
    lv_obj_align_to(header, hue_slider, LV_ALIGN_BOTTOM_MID, 0, 30);

    lv_obj_add_event_cb(hue_slider, light_hue_cb, LV_EVENT_ALL, NULL);
}

static void display_light(void) 
{
    display_light_bg();
    display_light_sw();
    display_light_header();
    display_light_brt();
    display_light_hue();
    set_sw(ON_OFF_SW);
}

void show_light_scrn(void) 
{
    if (lvgl_port_lock()) {
        lv_scr_load_anim(light_scrn_bg, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, false);
        lvgl_port_unlock();
    }
}

void display_light_screen(void) 
{
    if (lvgl_port_lock()) {
        display_light();
        lvgl_port_unlock();
    }
}

/**
 * This section is for relay device interface
 */
//=======================================================================================================================

static lv_obj_t *create_sw(lv_obj_t *parent, int width, int height, lv_align_t align, lv_coord_t x_offset,
                           lv_coord_t y_offset, int knob_color) 
{
    lv_obj_t *switch_obj = lv_switch_create(parent);
    lv_obj_set_size(switch_obj, width, height);
    lv_obj_align_to(switch_obj, parent, align, x_offset, y_offset);
    lv_obj_set_style_bg_color(switch_obj, lv_color_hex(knob_color), LV_PART_KNOB);
    return switch_obj;
}

static lv_obj_t *create_sw_label(lv_obj_t *parent, const char *text, lv_obj_t *base, lv_align_t align,
                                 lv_coord_t x_offset, lv_coord_t y_offset) 
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_align_to(label, base, align, x_offset, y_offset);
    return label;
}

static void display_relay_bg(void) 
{
    relay_scrn_bg = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(relay_scrn_bg, lv_color_hex(0xFFFF80), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(relay_scrn_bg, LV_OPA_COVER, LV_PART_MAIN);
}

static void display_relay_header(void) 
{
    relay_header = lv_label_create(relay_scrn_bg);
    lv_label_set_text(relay_header, "Relay device example");
    lv_obj_align(relay_header, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(relay_header, &lv_font_montserrat_12, LV_PART_MAIN);

    lv_obj_add_event_cb(relay_header, scrn_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(relay_header, relay_sw_select_cb, LV_EVENT_SHORT_CLICKED, NULL);
}

static void display_relay_sw(void) 
{
    relay_sw_1 = create_sw(relay_scrn_bg, 50, 25, LV_ALIGN_TOP_RIGHT, -12, 70, COLOR_ORANGE);
    create_sw_label(relay_scrn_bg, "SW 1", relay_sw_1, LV_ALIGN_BOTTOM_MID, 0, 20);
    relay_sw_2 = create_sw(relay_scrn_bg, 50, 25, LV_ALIGN_TOP_LEFT, 12, 70, COLOR_WHITE);
    create_sw_label(relay_scrn_bg, "SW 2", relay_sw_2, LV_ALIGN_BOTTOM_MID, 0, 20);
    relay_sw_3 = create_sw(relay_scrn_bg, 50, 25, LV_ALIGN_BOTTOM_LEFT, 12, -70, COLOR_WHITE);
    create_sw_label(relay_scrn_bg, "SW 3", relay_sw_3, LV_ALIGN_BOTTOM_MID, 0, 20);
    relay_sw_4 = create_sw(relay_scrn_bg, 50, 25, LV_ALIGN_BOTTOM_RIGHT, -12, -70, COLOR_WHITE);
    create_sw_label(relay_scrn_bg, "SW 4", relay_sw_4, LV_ALIGN_BOTTOM_MID, 0, 20);

    lv_obj_add_event_cb(relay_sw_1, relay_sw_cb, LV_EVENT_CLICKED, NULL);
}

static void display_relay(void) 
{
    display_relay_bg();
    display_relay_header();
    display_relay_sw();
    set_relay_sw(RELAY_SW_1);
}

void show_relay_scrn(void) 
{
    if (lvgl_port_lock()) {
        lv_scr_load_anim(relay_scrn_bg, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, false);
        lvgl_port_unlock();
    }
}

void display_relay_screen(void) 
{
    if (lvgl_port_lock()) {
        display_relay();
        lvgl_port_unlock();
    }
}
