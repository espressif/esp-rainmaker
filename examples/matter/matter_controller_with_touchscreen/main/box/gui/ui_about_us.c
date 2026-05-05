/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "esp_log.h"
#include "esp_idf_version.h"
#include "box_platform.h"
#include "lvgl.h"
#include "ui_main.h"
#include "ui_about_us.h"
#include "app_matter_ctrl.h"

#define LV_SYMBOL_EXTRA_SYNC "\xef\x80\xA1"
static bool perform_factory_reset = false;
static void (*g_about_us_end_cb)(void) = NULL;

static void ui_about_us_page_return_click_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_user_data(e);
    if (ui_get_btn_op_group()) {
        lv_group_remove_all_objs(ui_get_btn_op_group());
    }

    lv_obj_del(obj);
    // bsp_btn_rm_all_callback(BSP_BUTTON_MAIN);
    if (g_about_us_end_cb) {
        g_about_us_end_cb();
    }
}

static void timer_cb(struct _lv_timer_t *)
{
    matter_factory_reset();
}

static void msgbox_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *msgbox = lv_event_get_current_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        const char *txt = lv_msgbox_get_active_btn_text(msgbox);
        if (strcmp(txt, "Ok") == 0) {
            if (!perform_factory_reset) {
                lv_obj_t *text = lv_msgbox_get_text(msgbox);
                lv_label_set_text_fmt(text, "Do factory reset");
                lv_obj_set_style_text_color(text, lv_color_make(255, 0, 0), LV_STATE_DEFAULT);
                ESP_LOGI("BOX", "Factory reset triggered. Release the button to start factory reset.");
                perform_factory_reset = true;
                lv_timer_create(timer_cb, 2000, NULL);
            }
        } else {
            lv_msgbox_close(msgbox);
        }
    }
}

static void btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        static const char *btns[] = {"Ok", "Cancel", ""};
        lv_obj_t *mbox = lv_msgbox_create(NULL, "Setting", "Are you sure to reset Controller", btns, true);
        lv_obj_add_event_cb(mbox, msgbox_event_cb, LV_EVENT_ALL, NULL);
        lv_group_focus_obj(lv_msgbox_get_btns(mbox));
        lv_obj_add_state(lv_msgbox_get_btns(mbox), LV_STATE_FOCUS_KEY);
        lv_obj_align(mbox, LV_ALIGN_CENTER, 0, 0);
        lv_obj_t *bg = lv_obj_get_parent(mbox);
        lv_obj_set_style_bg_opa(bg, LV_OPA_70, 0);
        lv_obj_set_style_bg_color(bg, lv_palette_main(LV_PALETTE_GREY), 0);
    }
}

void ui_about_us_start(void (*fn)(void))
{
    g_about_us_end_cb = fn;

    lv_obj_t *page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(page, 290, 174);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(page, 15, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(page, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(page, 20, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(page, LV_OPA_30, LV_PART_MAIN);
    lv_obj_align_to(page, ui_main_get_status_bar(), LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_t *btn_return = lv_btn_create(page);
    lv_obj_set_size(btn_return, 24, 24);
    lv_obj_add_style(btn_return, &ui_button_styles()->style, 0);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_focus, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_focus, LV_STATE_FOCUSED);
    lv_obj_align(btn_return, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *lab_btn_text = lv_label_create(btn_return);
    lv_label_set_text_static(lab_btn_text, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lab_btn_text, lv_color_make(158, 158, 158), LV_STATE_DEFAULT);
    lv_obj_center(lab_btn_text);
    lv_obj_add_event_cb(btn_return, ui_about_us_page_return_click_cb, LV_EVENT_CLICKED, page);

    if (ui_get_btn_op_group()) {
        lv_group_add_obj(ui_get_btn_op_group(), btn_return);
    }

    lv_obj_t *img = lv_img_create(page);
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 20);
    LV_IMG_DECLARE(icon_box);
    lv_img_set_src(img, &icon_box);

    char msg[256] = {0};
    snprintf(msg, sizeof(msg),
             "#000000 Software Ver: # "  "#888888 V%u.%u.%u#\n"
             "#000000 ESP-IDF Ver: # "   "#888888 %s#\n"
             "#000000 Board: # "         "#888888 %s#",
             BOX_DEMO_VERSION_MAJOR, BOX_DEMO_VERSION_MINOR, BOX_DEMO_VERSION_PATCH,
             esp_get_idf_version(),
             box_platform_get_name());

    lv_obj_t *lab = lv_label_create(page);
    lv_label_set_recolor(lab, true);
    lv_label_set_text(lab, msg);
    lv_obj_align(lab, LV_ALIGN_BOTTOM_LEFT, 0, -10);

    lv_obj_t *reset_button = lv_btn_create(page);
    lv_obj_set_style_bg_color(reset_button, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(reset_button, lv_palette_lighten(LV_PALETTE_RED, 1), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(reset_button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(reset_button, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_align(reset_button, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(reset_button, 40, 40);
    lv_obj_t *reset_label = lv_label_create(page);
    lv_label_set_text_static(reset_label, LV_SYMBOL_EXTRA_SYNC);
    lv_obj_align_to(reset_label, reset_button, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(reset_button, btn_event_cb, LV_EVENT_CLICKED, NULL);

}
