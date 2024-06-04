/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "ui_matter_ctrl.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"

#if CONFIG_CUSTOM_COMMISSIONABLE_DATA_PROVIDER
#include "dynamic_qrcode.h"
#endif

static const char *TAG = "ui_matter_ctrl";

static bool IsCommission = false;

LV_IMG_DECLARE(icon_light_on)
LV_IMG_DECLARE(icon_light_off)
LV_IMG_DECLARE(icon_switch_on)
LV_IMG_DECLARE(icon_switch_off)
LV_IMG_DECLARE(icon_air_on)
LV_IMG_DECLARE(icon_air_off)
LV_IMG_DECLARE(icon_plug_on)
LV_IMG_DECLARE(icon_plug_off)

static ui_matter_state_t g_matter_state = UI_MATTER_EVT_LOADING;
static lv_obj_t *g_page = NULL;
static lv_obj_t *g_hint_label = NULL;
static lv_obj_t *QRcode = NULL;
static void (*g_dev_ctrl_end_cb)(void) = NULL;

static uint8_t qrcode_width = UI_SCALING(108);
static uint8_t qrcode_align_y = UI_SCALING(8);
static uint8_t btn_return_width = UI_SCALING(24);
static uint8_t hint_align_y = UI_SCALING(60);
static uint8_t control_button_width = 80;
static uint8_t control_button_height = 100;
static uint8_t control_button_x_interval = 90;
#if CONFIG_BSP_BOARD_ESP32_S3_LCD_EV_BOARD
static uint8_t control_button_first_row = UI_SCALING(60);
#else
static uint8_t control_button_first_row = UI_SCALING(40);
#endif
static int8_t image_align_y = -20;
static uint8_t name_align_y = 15;
static uint8_t online_align_y = 35;

typedef struct {
    const char *name;
    lv_img_dsc_t const *img_on;
    lv_img_dsc_t const *img_off;
} btn_img_src_t;

static const btn_img_src_t img_src_list[] = {
    {.name = "Light", .img_on = &icon_light_on, .img_off = &icon_light_off},
    {.name = "Plug", .img_on = &icon_plug_on, .img_off = &icon_plug_off},
    {.name = "Switch", .img_on = &icon_switch_on, .img_off = &icon_switch_off},
    {.name = "Unknown", .img_on = &icon_air_on, .img_off = &icon_air_off},
};

extern device_to_control_t device_to_control;

void ui_set_onoff_state(lv_obj_t *g_func_btn, size_t size_type, bool state)
{
    if (NULL == g_func_btn) {
        return;
    }
    lv_obj_t *img = (lv_obj_t *)g_func_btn->user_data;
    ui_acquire();
    if (state) {
        lv_obj_add_state(g_func_btn, LV_STATE_CHECKED);
        lv_img_set_src(img, img_src_list[size_type].img_on);
    } else {
        lv_obj_clear_state(g_func_btn, LV_STATE_CHECKED);
        lv_img_set_src(img, img_src_list[size_type].img_off);
    }
    ui_release();
}

static void device_image_click_cb(lv_event_t *e)
{
    node_endpoint_id_list_t *ptr = (node_endpoint_id_list_t *)lv_event_get_user_data(e);
    if (!ptr) {
        ESP_LOGI(TAG, "NULL ptr");
        return;
    }
    matter_ctrl_change_state((intptr_t)ptr);
}

static void ui_dev_ctrl_page_return_click_cb(lv_event_t *e)
{
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_user_data(e);
    if (ui_get_btn_op_group()) {
        lv_group_remove_all_objs(ui_get_btn_op_group());
    }

    lv_obj_del(obj);
    g_page = NULL;
    QRcode = NULL;
    matter_ctrl_lv_obj_clear();
    if (g_dev_ctrl_end_cb) {
        g_dev_ctrl_end_cb();
    }
}

static void ui_list_device(void)
{
    uint8_t num_of_device[4] = {0, 0, 0, 0};
    uint8_t kind_to_show = 0;
    uint8_t online_no = 0;
    uint8_t offline_no = device_to_control.online_num;
    matter_device_list_lock();
    node_endpoint_id_list_t *ptr = device_to_control.dev_list;
    while (ptr) {
        if(ptr->device_type == CONTROL_UNKNOWN_DEVICE)
        {
            ptr=ptr->next;
            continue;
        }
        lv_obj_t *g_func_btn = lv_btn_create(g_page);
        ptr->lv_obj = g_func_btn;
        lv_obj_set_size(g_func_btn, control_button_width, control_button_height);
        lv_obj_set_style_bg_color(g_func_btn, lv_color_white(), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_func_btn, lv_color_white(), LV_STATE_CHECKED);
        lv_obj_set_style_border_width(g_func_btn, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_func_btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_radius(g_func_btn, 10, LV_STATE_DEFAULT);

        ++kind_to_show;
        ++num_of_device[ptr->device_type];
        lv_obj_t *img = lv_img_create(g_func_btn);
        lv_obj_align(img, LV_ALIGN_CENTER, 0, image_align_y);
        lv_obj_set_user_data(g_func_btn, (void *)img);

        lv_obj_t *name_label = lv_label_create(g_func_btn);
        lv_label_set_text_fmt(name_label, "%s %d", img_src_list[ptr->device_type].name, num_of_device[ptr->device_type]);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
        lv_obj_align(name_label, LV_ALIGN_CENTER, 0, name_align_y);

        lv_obj_t *online_label = lv_label_create(g_func_btn);
        lv_obj_set_style_text_font(online_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
        lv_obj_align(online_label, LV_ALIGN_CENTER, 0, online_align_y);

        if (ptr->is_online) {
            if (ptr->OnOff) {
                ESP_LOGI(TAG, "device %llx is on\n", ptr->node_id);
                lv_img_set_src(img, img_src_list[ptr->device_type].img_on);
            } else {
                ESP_LOGI(TAG, "device %llx is off\n", ptr->node_id);
                lv_img_set_src(img, img_src_list[ptr->device_type].img_off);
            }

            lv_obj_set_style_text_color(name_label, lv_color_make(40, 40, 40), LV_STATE_DEFAULT);
            lv_label_set_text_static(online_label, "online");
            lv_obj_set_style_text_color(online_label, lv_color_make(40, 40, 40), LV_STATE_DEFAULT);
            lv_obj_add_event_cb(g_func_btn, device_image_click_cb, LV_EVENT_CLICKED, (void *)ptr);
            lv_obj_set_pos(g_func_btn, control_button_x_interval * online_no, control_button_first_row);
            ++online_no;
        } else {
            lv_img_set_src(img, img_src_list[ptr->device_type].img_off);
            lv_obj_set_style_text_color(name_label, lv_color_make(220, 220, 220), LV_STATE_DEFAULT);
            lv_label_set_text_static(online_label, "offline");
            lv_obj_set_style_text_color(online_label, lv_color_make(220, 220, 220), LV_STATE_DEFAULT);
            lv_obj_set_pos(g_func_btn, control_button_x_interval * offline_no, control_button_first_row);
            ++offline_no;
        }
        ptr = ptr->next;
    }

    if (0u == kind_to_show) {
        if (!g_hint_label) {
            g_hint_label = lv_label_create(g_page);
            lv_obj_set_style_text_color(g_hint_label, lv_color_make(40, 40, 40), LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(g_hint_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
            lv_obj_align(g_hint_label, LV_ALIGN_CENTER, 0, hint_align_y);
        }
        lv_label_set_text(g_hint_label, "No device list, please refresh");
        lv_obj_clear_flag(g_hint_label, LV_OBJ_FLAG_HIDDEN);
    }
    matter_device_list_unlock();
}

void ui_matter_config_update_cb(ui_matter_state_t state)
{
    g_matter_state = state;

    ESP_LOGI(TAG, "UI state: %d", g_matter_state);

    if (!g_page) {
        return;
    }

    ui_acquire();
    switch (state) {
    case UI_MATTER_EVT_LOADING:
        if (g_hint_label) {
            lv_obj_clear_flag(g_hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(g_hint_label, "Scan the QR code on your phone");
        }
        break;
    case UI_MATTER_EVT_START_COMMISSION:
        if (g_hint_label) {
            lv_obj_clear_flag(g_hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(g_hint_label, "Start Commission ...");
        }
        break;
    case UI_MATTER_EVT_FAILED_COMMISSION:
        if (QRcode) {
            lv_obj_clear_flag(QRcode, LV_OBJ_FLAG_HIDDEN);
        }
        if (g_hint_label) {
            lv_obj_clear_flag(g_hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(g_hint_label, "Failed commission ...");
        }
        break;
    case UI_MATTER_EVT_COMMISSIONCOMPLETE:
    case UI_MATTER_EVT_REFRESH:
        IsCommission = true;
        if (QRcode) {
            lv_obj_add_flag(QRcode, LV_OBJ_FLAG_HIDDEN);
        }
        if (g_hint_label) {
            lv_obj_add_flag(g_hint_label, LV_OBJ_FLAG_HIDDEN);
        }
        ui_list_device();
        break;
    default:
        break;
    }
    ui_release();
}

void clean_screen_with_button(void)
{
    if (!g_page) {
        return;
    }
    ui_acquire();
    lv_obj_clean(g_page);
    QRcode = NULL;
    g_hint_label = NULL;
    lv_obj_t *btn_return = lv_btn_create(g_page);
    lv_obj_set_size(btn_return, btn_return_width, btn_return_width);
    lv_obj_add_style(btn_return, &ui_button_styles()->style, 0);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_focus, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_focus, LV_STATE_FOCUSED);
    lv_obj_align(btn_return, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *lab_btn_text = lv_label_create(btn_return);
#if CONFIG_BSP_BOARD_ESP32_S3_LCD_EV_BOARD
    lv_obj_set_style_text_font(lab_btn_text, &lv_font_montserrat_24, LV_PART_MAIN);
#endif
    lv_label_set_text_static(lab_btn_text, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lab_btn_text, lv_color_make(158, 158, 158), LV_STATE_DEFAULT);
    lv_obj_center(lab_btn_text);
    lv_obj_add_event_cb(btn_return, ui_dev_ctrl_page_return_click_cb, LV_EVENT_CLICKED, g_page);
    lv_obj_add_flag(btn_return, LV_OBJ_FLAG_FLOATING);

    if (ui_get_btn_op_group()) {
        lv_group_add_obj(ui_get_btn_op_group(), btn_return);
    }
    ui_release();
}

void ui_matter_ctrl_start(void (*fn)(void))
{
    g_dev_ctrl_end_cb = fn;

    g_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_page, UI_SCALING(UI_PAGE_H_RES), UI_SCALING(174));
    lv_obj_set_style_radius(g_page, 15, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_page, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_page, 20, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(g_page, LV_OPA_30, LV_PART_MAIN);
    lv_obj_align_to(g_page, ui_main_get_status_bar(), LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_t *btn_return = lv_btn_create(g_page);
    lv_obj_set_size(btn_return, btn_return_width, btn_return_width);
    lv_obj_add_style(btn_return, &ui_button_styles()->style, 0);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_focus, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_focus, LV_STATE_FOCUSED);
    lv_obj_align(btn_return, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *lab_btn_text = lv_label_create(btn_return);
#if CONFIG_BSP_BOARD_ESP32_S3_LCD_EV_BOARD
    lv_obj_set_style_text_font(lab_btn_text, &lv_font_montserrat_24, LV_PART_MAIN);
#endif
    lv_label_set_text_static(lab_btn_text, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lab_btn_text, lv_color_make(158, 158, 158), LV_STATE_DEFAULT);
    lv_obj_center(lab_btn_text);
    lv_obj_add_event_cb(btn_return, ui_dev_ctrl_page_return_click_cb, LV_EVENT_CLICKED, g_page);
    lv_obj_add_flag(btn_return, LV_OBJ_FLAG_FLOATING);

    g_hint_label = lv_label_create(g_page);
    lv_obj_set_style_text_color(g_hint_label, lv_color_make(40, 40, 40), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_hint_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_align(g_hint_label, LV_ALIGN_CENTER, 0, hint_align_y);

    if (!IsCommission) {
        QRcode = lv_qrcode_create(g_page, qrcode_width, lv_color_black(), lv_color_white());
        lv_obj_align(QRcode, LV_ALIGN_TOP_MID, 0, qrcode_align_y);
#ifdef CONFIG_CUSTOM_COMMISSIONABLE_DATA_PROVIDER
        const char *qrcode_data = DynamicPasscodeCommissionableDataProvider::GetInstance().GetDynamicQRcodeStr();
#else
        const char *qrcode_data = "MT:U9VJ0EPJ01ZD6100000";
#endif
        ESP_LOGI(TAG, "QR Data: %s", qrcode_data);
        lv_qrcode_update(QRcode, qrcode_data, strlen(qrcode_data));
        lv_label_set_text_static(g_hint_label, "Scan the QR code on your phone");
    }
    ESP_LOGI(TAG, "Current Free Memory Internal:\t%d\t SPIRAM:%d",
             heap_caps_get_free_size(MALLOC_CAP_8BIT) - heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    ui_matter_config_update_cb(g_matter_state);
}
