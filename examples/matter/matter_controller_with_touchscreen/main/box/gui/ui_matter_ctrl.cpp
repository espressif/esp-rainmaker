/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <app_matter_ctrl.h>
#include <app_matter_device_list.h>
#include <app_matter_device_types.h>
#include <app_matter_view_model.h>
#include <app_rmaker_matter_report.h>
#include <app_rmaker_matter_device_list.h>
#include <devices/onoff.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <lvgl.h>
#include <sdkconfig.h>
#include <ui_matter_ctrl.h>

#include <stdlib.h>
#include <string.h>

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
static lv_obj_t *g_qr_text = NULL;
static lv_obj_t *g_refresh_btn = NULL;
static lv_obj_t *g_refresh_label = NULL;
static lv_timer_t *g_refresh_cooldown_timer = NULL;
static bool g_refresh_cooling_down = false;
static bool g_scroll_active = false;
static bool g_refresh_deferred = false;
static lv_coord_t g_saved_scroll_x = 0;

static void set_qr_payload(const char *qrcode_data);
static void clean_screen_with_button_locked(void);
static void ui_list_device(void);

static void save_scroll_x_locked(void)
{
    if (g_page) {
        g_saved_scroll_x = lv_obj_get_scroll_x(g_page);
    }
}

static void rebuild_device_list_locked(bool save_current_scroll)
{
    IsCommission = true;
    if (save_current_scroll) {
        save_scroll_x_locked();
    }
    clean_screen_with_button_locked();
    ui_list_device();
    lv_obj_scroll_to_x(g_page, g_saved_scroll_x, LV_ANIM_OFF);
}

static uint8_t qrcode_width = 124;
static uint8_t qrcode_align_y = 5;
static uint8_t btn_return_width = 24;
static uint8_t hint_align_y = 60;
static uint8_t control_button_width = 80;
static uint8_t control_button_height = 100;
static uint8_t control_button_x_interval = 90;
static uint8_t control_button_first_row = 40;
static int8_t image_align_y = -20;
static uint8_t name_align_y = 15;
static uint8_t online_align_y = 35;
static constexpr uint32_t kRefreshCooldownMs = 3000;
static matter_device_vm_item_t s_device_snapshot[CONFIG_RMAKER_MTCTL_MAX_DEVICE_COUNT];

typedef struct {
    uint64_t node_id;
    uint16_t endpoint_id;
    matter_device_type_t device_type;
    lv_obj_t *button;
    lv_obj_t *image;
    lv_obj_t *online_label;
} device_card_t;

static device_card_t s_device_cards[CONFIG_RMAKER_MTCTL_MAX_DEVICE_COUNT];
static size_t s_device_card_count;

static void *ui_calloc(size_t num, size_t size)
{
    return heap_caps_calloc_prefer(num, size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM,
                                   MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
}

typedef struct {
    uint64_t node_id;
    uint16_t endpoint_id;
} device_click_target_t;

typedef struct {
    const char *name;
    lv_image_dsc_t const *img_on;
    lv_image_dsc_t const *img_off;
} btn_img_src_t;

static const btn_img_src_t img_src_list[] = {
    {.name = "Light", .img_on = &icon_light_on, .img_off = &icon_light_off},
    {.name = "Plug", .img_on = &icon_plug_on, .img_off = &icon_plug_off},
    {.name = "Switch", .img_on = &icon_switch_on, .img_off = &icon_switch_off},
    {.name = "Unknown", .img_on = &icon_air_on, .img_off = &icon_air_off},
};

static device_card_t *find_device_card(uint64_t node_id, uint16_t endpoint_id)
{
    for (size_t i = 0; i < s_device_card_count; ++i) {
        device_card_t *card = &s_device_cards[i];
        if (card->node_id == node_id && card->endpoint_id == endpoint_id) {
            return card;
        }
    }
    return NULL;
}

static void reset_device_cards(void)
{
    memset(s_device_cards, 0, sizeof(s_device_cards));
    s_device_card_count = 0;
}

static void update_device_card_locked(device_card_t *card, const matter_device_vm_item_t *item)
{
    if (!card || !item || !card->button || !card->image || !card->online_label) {
        return;
    }

    const bool on = item->is_online && matter_device_type_is_onoff(item->device_type) && item->state.onoff.onoff;
    lv_img_set_src(card->image, on ? img_src_list[item->device_type].img_on : img_src_list[item->device_type].img_off);

    if (item->is_online) {
        lv_obj_set_style_text_color(card->online_label, lv_color_make(40, 40, 40), LV_STATE_DEFAULT);
        lv_label_set_text_static(card->online_label, "online");
    } else {
        lv_obj_set_style_text_color(card->online_label, lv_color_make(220, 220, 220), LV_STATE_DEFAULT);
        lv_label_set_text_static(card->online_label, "offline");
    }
}

static void device_image_click_cb(lv_event_t *e)
{
    device_click_target_t *target = (device_click_target_t *)lv_event_get_user_data(e);
    if (!target) {
        ESP_LOGI(TAG, "NULL ptr");
        return;
    }
    matter_ctrl_primary_action(target->node_id, target->endpoint_id);
}

static void device_target_delete_cb(lv_event_t *e)
{
    free(lv_event_get_user_data(e));
}

static void ui_dev_ctrl_page_return_click_cb(lv_event_t *e)
{
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_user_data(e);
    if (ui_get_btn_op_group()) {
        lv_group_remove_all_objs(ui_get_btn_op_group());
    }

    save_scroll_x_locked();
    lv_obj_del(obj);
    g_page = NULL;
    QRcode = NULL;
    g_qr_text = NULL;
    g_refresh_btn = NULL;
    g_refresh_label = NULL;
    reset_device_cards();
    if (g_dev_ctrl_end_cb) {
        g_dev_ctrl_end_cb();
    }
}

static void update_refresh_button_state(void)
{
    if (!g_refresh_btn || !g_refresh_label) {
        return;
    }

    if (g_refresh_cooling_down || !app_rmaker_matter_device_list_updatable()) {
        lv_obj_add_state(g_refresh_btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(g_refresh_btn, lv_color_make(230, 230, 230), LV_STATE_DISABLED);
        lv_obj_set_style_text_color(g_refresh_label, lv_color_make(180, 180, 180), LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(g_refresh_btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(g_refresh_btn, lv_color_white(), LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(g_refresh_label, lv_color_make(158, 158, 158), LV_STATE_DEFAULT);
    }
}

static void refresh_cooldown_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    g_refresh_cooldown_timer = NULL;
    g_refresh_cooling_down = false;
    update_refresh_button_state();
}

static void start_refresh_cooldown(void)
{
    g_refresh_cooling_down = true;
    if (g_refresh_cooldown_timer) {
        lv_timer_reset(g_refresh_cooldown_timer);
    } else {
        g_refresh_cooldown_timer = lv_timer_create(refresh_cooldown_timer_cb, kRefreshCooldownMs, NULL);
        lv_timer_set_repeat_count(g_refresh_cooldown_timer, 1);
    }
    update_refresh_button_state();
}

static void refresh_click_cb(lv_event_t *e)
{
    (void)e;
    if (g_refresh_cooling_down) {
        return;
    }

    start_refresh_cooldown();
    esp_err_t err = app_rmaker_matter_report_retry_subscriptions();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Failed to retry Matter attr subscriptions: %s", esp_err_to_name(err));
    }
    matter_device_list_fetch();
}

static void page_scroll_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SCROLL_BEGIN) {
        g_scroll_active = true;
        return;
    }

    if (code != LV_EVENT_SCROLL_END) {
        return;
    }

    g_scroll_active = false;
    if (g_refresh_deferred) {
        g_refresh_deferred = false;
        rebuild_device_list_locked(true);
    }
}

static void create_refresh_button(void)
{
    if (!g_refresh_btn) {
        g_refresh_btn = lv_btn_create(g_page);
        lv_obj_set_size(g_refresh_btn, btn_return_width, btn_return_width);
        lv_obj_align(g_refresh_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_add_style(g_refresh_btn, &ui_button_styles()->style, 0);
        lv_obj_add_style(g_refresh_btn, &ui_button_styles()->style_pr, LV_STATE_PRESSED);
        lv_obj_add_style(g_refresh_btn, &ui_button_styles()->style_focus, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(g_refresh_btn, &ui_button_styles()->style_focus, LV_STATE_FOCUSED);
        lv_obj_add_event_cb(g_refresh_btn, refresh_click_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_flag(g_refresh_btn, LV_OBJ_FLAG_FLOATING);

        g_refresh_label = lv_label_create(g_refresh_btn);
        lv_label_set_text_static(g_refresh_label, LV_SYMBOL_REFRESH);
        lv_obj_set_style_text_font(g_refresh_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
        lv_obj_center(g_refresh_label);
    }
    update_refresh_button_state();
}

static void ui_list_device(void)
{
    uint8_t num_of_device[4] = {0, 0, 0, 0};
    uint8_t kind_to_show = 0;
    uint8_t online_no = 0;
    matter_device_vm_status_t status = {};
    matter_vm_get_status(&status);
    uint8_t offline_no = status.online_count;
    size_t device_count = 0;
    matter_vm_copy_devices(s_device_snapshot, CONFIG_RMAKER_MTCTL_MAX_DEVICE_COUNT, &device_count);

    create_refresh_button();
    for (size_t i = 0; i < device_count; ++i) {
        matter_device_vm_item_t *item = &s_device_snapshot[i];
        if (item->device_type == MATTER_DEVICE_TYPE_UNKNOWN) {
            continue;
        }
        lv_obj_t *g_func_btn = lv_btn_create(g_page);
        lv_obj_clear_flag(g_func_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(g_func_btn, control_button_width, control_button_height);
        lv_obj_set_style_bg_color(g_func_btn, lv_color_white(), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_func_btn, lv_color_white(), LV_STATE_CHECKED);
        lv_obj_set_style_border_width(g_func_btn, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_func_btn, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_obj_set_style_radius(g_func_btn, 10, LV_STATE_DEFAULT);

        ++kind_to_show;
        ++num_of_device[item->device_type];
        lv_obj_t *img = lv_img_create(g_func_btn);
        lv_obj_align(img, LV_ALIGN_CENTER, 0, image_align_y);
        lv_obj_set_user_data(g_func_btn, (void *)img);

        lv_obj_t *name_label = lv_label_create(g_func_btn);
        lv_label_set_text_fmt(name_label, "%s %d", matter_device_type_label(item->device_type),
                              num_of_device[item->device_type]);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
        lv_obj_align(name_label, LV_ALIGN_CENTER, 0, name_align_y);

        lv_obj_t *online_label = lv_label_create(g_func_btn);
        lv_obj_set_style_text_font(online_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
        lv_obj_align(online_label, LV_ALIGN_CENTER, 0, online_align_y);

        device_card_t *card = NULL;
        if (s_device_card_count < CONFIG_RMAKER_MTCTL_MAX_DEVICE_COUNT) {
            card = &s_device_cards[s_device_card_count++];
            card->node_id = item->node_id;
            card->endpoint_id = item->endpoint_id;
            card->device_type = item->device_type;
            card->button = g_func_btn;
            card->image = img;
            card->online_label = online_label;
        }

        if (item->is_online) {
            lv_obj_set_style_text_color(name_label, lv_color_make(40, 40, 40), LV_STATE_DEFAULT);
            device_click_target_t *target = (device_click_target_t *)ui_calloc(1, sizeof(device_click_target_t));
            if (target) {
                target->node_id = item->node_id;
                target->endpoint_id = item->endpoint_id;
                lv_obj_add_event_cb(g_func_btn, device_image_click_cb, LV_EVENT_CLICKED, target);
                lv_obj_add_event_cb(g_func_btn, device_target_delete_cb, LV_EVENT_DELETE, target);
            }
            lv_obj_set_pos(g_func_btn, control_button_x_interval * online_no, control_button_first_row);
            ++online_no;
        } else {
            lv_obj_set_style_text_color(name_label, lv_color_make(220, 220, 220), LV_STATE_DEFAULT);
            lv_obj_set_pos(g_func_btn, control_button_x_interval * offline_no, control_button_first_row);
            ++offline_no;
        }
        update_device_card_locked(card, item);
    }

    if (0u == kind_to_show) {
        if (!g_hint_label) {
            g_hint_label = lv_label_create(g_page);
            lv_obj_set_style_text_color(g_hint_label, lv_color_make(40, 40, 40), LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(g_hint_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
            lv_obj_align(g_hint_label, LV_ALIGN_CENTER, 0, hint_align_y);
        }
        lv_label_set_text(g_hint_label, app_rmaker_matter_device_list_updatable()
                                      ? "No device list, tap Refresh"
                                      : "Controller setup not ready yet");
        lv_obj_clear_flag(g_hint_label, LV_OBJ_FLAG_HIDDEN);
    }
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
    case UI_MATTER_EVT_PROVISIONING:
        if (g_hint_label) {
            lv_obj_clear_flag(g_hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(g_hint_label, "Waiting for RainMaker provisioning");
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
    case UI_MATTER_EVT_REFRESH: {
        if (g_scroll_active) {
            g_refresh_deferred = true;
            break;
        }
        rebuild_device_list_locked(true);
        break;
    }
    default:
        break;
    }
    ui_release();
}

void ui_matter_device_state_update(uint64_t node_id, uint16_t endpoint_id)
{
    if (!g_page) {
        return;
    }

    matter_device_vm_item_t item = {};
    if (!matter_vm_get_device(node_id, endpoint_id, &item) || item.device_type == MATTER_DEVICE_TYPE_UNKNOWN) {
        return;
    }

    ui_acquire();
    device_card_t *card = find_device_card(node_id, endpoint_id);
    if (card) {
        update_device_card_locked(card, &item);
    }
    ui_release();
}

static void clean_screen_with_button_locked(void)
{
    if (!g_page) {
        return;
    }
    lv_obj_clean(g_page);
    QRcode = NULL;
    g_hint_label = NULL;
    g_qr_text = NULL;
    g_refresh_btn = NULL;
    g_refresh_label = NULL;
    reset_device_cards();
    lv_obj_t *btn_return = lv_btn_create(g_page);
    lv_obj_set_size(btn_return, btn_return_width, btn_return_width);
    lv_obj_add_style(btn_return, &ui_button_styles()->style, 0);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_focus, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_focus, LV_STATE_FOCUSED);
    lv_obj_align(btn_return, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *lab_btn_text = lv_label_create(btn_return);
    lv_label_set_text_static(lab_btn_text, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lab_btn_text, lv_color_make(158, 158, 158), LV_STATE_DEFAULT);
    lv_obj_center(lab_btn_text);
    lv_obj_add_event_cb(btn_return, ui_dev_ctrl_page_return_click_cb, LV_EVENT_CLICKED, g_page);
    lv_obj_add_flag(btn_return, LV_OBJ_FLAG_FLOATING);

    if (ui_get_btn_op_group()) {
        lv_group_add_obj(ui_get_btn_op_group(), btn_return);
    }
}

void ui_matter_ctrl_start(void (*fn)(void))
{
    g_dev_ctrl_end_cb = fn;
    control_button_first_row = 40;

    g_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_page, 290, 174);
    lv_obj_set_scroll_dir(g_page, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(g_page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(g_page, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_style_radius(g_page, 15, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_page, 0, LV_PART_MAIN);
    lv_obj_align_to(g_page, ui_main_get_status_bar(), LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(g_page, page_scroll_cb, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(g_page, page_scroll_cb, LV_EVENT_SCROLL_END, NULL);

    lv_obj_t *btn_return = lv_btn_create(g_page);
    lv_obj_set_size(btn_return, btn_return_width, btn_return_width);
    lv_obj_add_style(btn_return, &ui_button_styles()->style, 0);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_focus, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_focus, LV_STATE_FOCUSED);
    lv_obj_align(btn_return, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *lab_btn_text = lv_label_create(btn_return);
    lv_label_set_text_static(lab_btn_text, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lab_btn_text, lv_color_make(158, 158, 158), LV_STATE_DEFAULT);
    lv_obj_center(lab_btn_text);
    lv_obj_add_event_cb(btn_return, ui_dev_ctrl_page_return_click_cb, LV_EVENT_CLICKED, g_page);
    lv_obj_add_flag(btn_return, LV_OBJ_FLAG_FLOATING);

    g_hint_label = lv_label_create(g_page);
    lv_obj_set_style_text_color(g_hint_label, lv_color_make(40, 40, 40), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_hint_label, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_align(g_hint_label, LV_ALIGN_TOP_MID, 0, qrcode_align_y + qrcode_width + 6);

    if (!IsCommission) {
        const char *qrcode_data = matter_ctrl_get_qr_payload();
        if (qrcode_data && qrcode_data[0]) {
            set_qr_payload(qrcode_data);
            lv_label_set_text_static(g_hint_label, "Scan the QR code on your phone");
        } else {
            lv_label_set_text_static(g_hint_label, "Provisioning QR not ready yet");
        }
    }
    ESP_LOGI(TAG, "Current Free Memory Internal:\t%d\t SPIRAM:%d",
             heap_caps_get_free_size(MALLOC_CAP_8BIT) - heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    if (g_matter_state == UI_MATTER_EVT_COMMISSIONCOMPLETE || g_matter_state == UI_MATTER_EVT_REFRESH) {
        ui_acquire();
        rebuild_device_list_locked(false);
        ui_release();
    } else {
        ui_matter_config_update_cb(g_matter_state);
    }
}

static void set_qr_payload(const char *qrcode_data)
{
    if (!QRcode) {
        QRcode = lv_qrcode_create(g_page);
        lv_qrcode_set_size(QRcode, qrcode_width);
        lv_qrcode_set_dark_color(QRcode, lv_color_black());
        lv_qrcode_set_light_color(QRcode, lv_color_white());
        lv_obj_align(QRcode, LV_ALIGN_TOP_MID, 0, qrcode_align_y);
    }

    if (lv_qrcode_update(QRcode, qrcode_data, strlen(qrcode_data)) == LV_RESULT_OK) {
        lv_obj_clear_flag(QRcode, LV_OBJ_FLAG_HIDDEN);
        if (g_qr_text) {
            lv_obj_add_flag(g_qr_text, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    if (!g_qr_text) {
        g_qr_text = lv_label_create(g_page);
        lv_obj_set_width(g_qr_text, 250);
        lv_label_set_long_mode(g_qr_text, LV_LABEL_LONG_WRAP);
        lv_obj_align(g_qr_text, LV_ALIGN_TOP_MID, 0, qrcode_align_y + 8);
    }
    lv_label_set_text(g_qr_text, qrcode_data);
    lv_obj_clear_flag(g_qr_text, LV_OBJ_FLAG_HIDDEN);
}
