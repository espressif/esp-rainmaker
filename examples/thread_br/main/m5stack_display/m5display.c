/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "m5display.h"

#if CONFIG_M5STACK_THREAD_BR_BOARD
#include <app_insights.h>
#include <app_thread_config.h>
#include <app_wifi.h>
#include <bsp/esp-bsp.h>
#include <esp_openthread_lock.h>
#include <esp_rmaker_thread_br.h>
#include <esp_rmaker_utils.h>
#include <esp_wifi.h>
#include <lvgl.h>

static const char *TAG = "m5_display";
static lv_obj_t *page = NULL;
static lv_obj_t *qrcode = NULL;
static lv_obj_t *tips_label = NULL;
static lv_obj_t *thread_panid_text = NULL;
static lv_obj_t *waiting_label = NULL;
static lv_obj_t *wifi_ssid_text = NULL;
static char *qr_data = NULL;
static bool br_info_gotten_flag = false;
static lv_obj_t *br_info_label = NULL;
TaskHandle_t xRefresh_Ui_Handle = NULL;
LV_IMG_DECLARE(Thread)
LV_IMG_DECLARE(wifi)
LV_IMG_DECLARE(logo)
#define LV_SYMBOL_EXTRA_SYNC "\xef\x80\xA1"

typedef struct border_router_info {
    const char *network_name;
    otPanId pan_id;
    uint64_t ext_pan_id;
    uint8_t channel;
    wifi_config_t stationConfig;
} border_router_info_t;

static border_router_info_t *get_border_router_info()
{
    static border_router_info_t info;
    if (!br_info_gotten_flag) {
        esp_openthread_lock_acquire(portMAX_DELAY);
        otInstance *instance = esp_openthread_get_instance();
        info.network_name = otThreadGetNetworkName(instance);
        info.channel = otLinkGetChannel(instance);
        info.pan_id = otLinkGetPanId(instance);
        const otExtendedPanId *extended_pan_id = otThreadGetExtendedPanId(instance);
        esp_openthread_lock_release();
        info.ext_pan_id = 0;
        for (size_t i = 0; i < OT_EXT_PAN_ID_SIZE; ++i) {
            info.ext_pan_id |= (uint64_t)(extended_pan_id->m8[i]) << (56 - 8 * i);
        }
        br_info_gotten_flag = true;
        esp_wifi_get_config(WIFI_IF_STA, &info.stationConfig);
    }
    return &info;
}

static void msg_btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *mbox = lv_obj_get_parent(lv_obj_get_parent(btn));
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    static bool factory_reset = false;
    if (strcmp(lv_label_get_text(label), "Ok") == 0) {
        if (!factory_reset) {
            lv_obj_t *content = lv_msgbox_get_content(mbox);
            lv_obj_t *label = lv_label_create(content);
            lv_label_set_text_static(label, "Reseting");
            lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_RED), LV_STATE_DEFAULT);
            esp_rmaker_factory_reset(1, 0);
            factory_reset = true;
        }
    } else {
        lv_msgbox_close(mbox);
    }
}

static void btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *msgbox = lv_msgbox_create(NULL);
        lv_msgbox_add_title(msgbox, "Setting");
        lv_msgbox_add_text(msgbox, "Are you sure to reset BR?");
        lv_obj_t *btn = lv_msgbox_add_footer_button(msgbox, "Ok");
        lv_obj_set_size(btn, 80, 40);
        lv_obj_add_event_cb(btn, msg_btn_event_cb, LV_EVENT_CLICKED, NULL);
        btn = lv_msgbox_add_footer_button(msgbox, "Cancel");
        lv_obj_set_size(btn, 80, 40);
        lv_obj_add_event_cb(btn, msg_btn_event_cb, LV_EVENT_CLICKED, NULL);
    }
}

static void msgbox_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        br_info_label = NULL;
    }
}

static void show_border_router_info_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *msgbox = lv_msgbox_create(NULL);
        lv_msgbox_add_title(msgbox, "Thread border router info");
        lv_obj_t *content = lv_msgbox_get_content(msgbox);
        br_info_label = lv_label_create(content);
        border_router_info_t *info = get_border_router_info();
        lv_label_set_text_fmt(br_info_label, "name: %s\rchannel: %d\rpanid: 0x%x\rextpanid: %llx", info->network_name,
                              info->channel, info->pan_id, info->ext_pan_id);
        lv_msgbox_add_close_button(msgbox);
        lv_obj_add_event_cb(msgbox, msgbox_event_cb, LV_EVENT_DELETE, NULL);
    }
}

static void animation_opa_ready_callback(lv_anim_t *anim)
{
    lv_anim_start(lv_anim_get_user_data(anim));
}

static void animation_move_ready_callback(lv_anim_t *anim)
{
    if (xRefresh_Ui_Handle) {
        xTaskNotifyGive(xRefresh_Ui_Handle);
    }
}

static void anim_opa_exec_cb(void *obj, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, value, LV_STATE_DEFAULT);
}

static void anim_move_exec_cb(void *obj, int32_t value)
{
    lv_obj_set_x((lv_obj_t *)obj, value * 0.5);
    lv_obj_set_y((lv_obj_t *)obj, value);
}

static void display_animation_task(void *args)
{
    page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(page, 320, 240);
    lv_obj_align(page, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *espressif_img = lv_img_create(page);
    lv_obj_set_style_opa(espressif_img, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_align(espressif_img, LV_ALIGN_CENTER, 0, 0);
    lv_img_set_src(espressif_img, &logo);

    lv_anim_t anim_1;
    lv_anim_init(&anim_1);
    lv_anim_set_var(&anim_1, espressif_img);
    lv_anim_set_values(&anim_1, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&anim_1, 1000);
    lv_anim_set_exec_cb(&anim_1, anim_opa_exec_cb);
    lv_anim_set_ready_cb(&anim_1, animation_opa_ready_callback);

    lv_anim_t anim_2;
    lv_anim_init(&anim_2);
    lv_anim_set_var(&anim_2, espressif_img);
    lv_anim_set_values(&anim_2, 0, -85);
    lv_anim_set_time(&anim_2, 500);
    lv_anim_set_exec_cb(&anim_2, anim_move_exec_cb);
    lv_anim_set_ready_cb(&anim_2, animation_move_ready_callback);
    lv_anim_set_user_data(&anim_1, &anim_2);
    lv_anim_start(&anim_1);
    vTaskDelete(NULL);
}

static void qr_display_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    bsp_display_lock(0);
    if (waiting_label && !lv_obj_has_flag(waiting_label, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(waiting_label, LV_OBJ_FLAG_HIDDEN);
    }
    bsp_display_unlock();
    if (event_base == APP_NETWORK_EVENT && event_id == APP_NETWORK_EVENT_QR_DISPLAY) {
        qr_data = (char *)event_data;
        bsp_display_lock(0);
        qrcode = lv_qrcode_create(page);
        lv_obj_align(qrcode, LV_ALIGN_CENTER, 0, 10);
        lv_qrcode_update(qrcode, qr_data, strlen(qr_data));
        tips_label = lv_label_create(page);
        lv_label_set_text_static(tips_label, "Use RainMaker to setup BR");
        lv_obj_align_to(tips_label, qrcode, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
        bsp_display_unlock();
    } else if (event_base == OPENTHREAD_EVENT && event_id == OPENTHREAD_EVENT_ROLE_CHANGED) {
        if (!thread_panid_text) {
            border_router_info_t *info = get_border_router_info();
            bsp_display_lock(0);
            lv_obj_t *thread_img = lv_img_create(page);
            lv_obj_align(thread_img, LV_ALIGN_CENTER, 90, 80);
            lv_img_set_src(thread_img, &Thread);

            thread_panid_text = lv_label_create(page);
            lv_label_set_text_fmt(thread_panid_text, "panid:0x%x", info->pan_id);
            lv_obj_align_to(thread_panid_text, thread_img, LV_ALIGN_OUT_TOP_MID, 0, -10);

            lv_obj_t *border_router_obj = lv_btn_create(page);
            lv_obj_set_size(border_router_obj, 280, 60);
            lv_obj_set_style_bg_color(border_router_obj, lv_color_white(), LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(border_router_obj, lv_palette_lighten(LV_PALETTE_BLUE, 1), LV_STATE_PRESSED);
            lv_obj_set_style_border_width(border_router_obj, 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(border_router_obj, lv_color_black(), LV_PART_MAIN);
            lv_obj_add_event_cb(border_router_obj, show_border_router_info_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_align(border_router_obj, LV_ALIGN_CENTER, 0, -15);

            lv_obj_t *border_router_label = lv_label_create(page);
            lv_label_set_text_static(border_router_label, "ESP Thread Border Router");
            lv_obj_set_style_text_font(border_router_label, &lv_font_montserrat_20, LV_PART_MAIN);
            lv_obj_align_to(border_router_label, border_router_obj, LV_ALIGN_CENTER, 0, 0);

            lv_obj_t *wifi_img = lv_img_create(page);
            lv_obj_align(wifi_img, LV_ALIGN_CENTER, -90, 80);
            lv_img_set_src(wifi_img, &wifi);

            wifi_ssid_text = lv_label_create(page);
            lv_label_set_text_fmt(wifi_ssid_text, "ssid:%s", (char *)(info->stationConfig.sta.ssid));
            lv_obj_align_to(wifi_ssid_text, wifi_img, LV_ALIGN_OUT_TOP_MID, 0, -10);

            if (qrcode) {
                lv_obj_add_flag(qrcode, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(tips_label, LV_OBJ_FLAG_HIDDEN);
            }
            bsp_display_unlock();
        }
    } else if (event_base == OPENTHREAD_EVENT && event_id == OPENTHREAD_EVENT_DATASET_CHANGED) {
        br_info_gotten_flag = false;
        border_router_info_t *info = get_border_router_info();
        bsp_display_lock(0);
        if (thread_panid_text) {
            lv_label_set_text_fmt(thread_panid_text, "panid:0x%x", info->pan_id);
        }
        if (br_info_label) {
            lv_label_set_text_fmt(br_info_label, "name: %s\rchannel: %d\rpanid: 0x%x\rextpanid: %llx",
                                  info->network_name, info->channel, info->pan_id, info->ext_pan_id);
        }
        bsp_display_unlock();
    }
}

void app_m5stack_display_start()
{
    bsp_i2c_init();
    bsp_display_cfg_t cfg = {.lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
                             .buffer_size = BSP_LCD_H_RES * 10,
                             .double_buffer = 0,
                             .flags = {
                                 .buff_dma = true,
                                 .buff_spiram = false,
                             }};
    cfg.lvgl_port_cfg.task_affinity = 1;
    bsp_display_start_with_config(&cfg);

    xTaskCreatePinnedToCore(display_animation_task, "AnimationTask", 2048, NULL, 1, NULL, 1);
    xRefresh_Ui_Handle = xTaskGetCurrentTaskHandle();
    while (true) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == true) {
            /* Wait for m5 stack display start */
            break;
        }
    }
    ESP_LOGI(TAG, "m5 stack animation finish, exec next");
    lv_obj_t *reset_button = lv_btn_create(page);
    lv_obj_set_style_bg_color(reset_button, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(reset_button, lv_palette_lighten(LV_PALETTE_RED, 1), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(reset_button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(reset_button, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_align(reset_button, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(reset_button, 60, 40);
    lv_obj_t *reset_label = lv_label_create(page);
    lv_label_set_text_static(reset_label, LV_SYMBOL_EXTRA_SYNC);
    lv_obj_align_to(reset_label, reset_button, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(reset_button, btn_event_cb, LV_EVENT_CLICKED, NULL);

    waiting_label = lv_label_create(page);
    lv_label_set_text_static(waiting_label, "Waiting for BR initialization");
    lv_obj_align(waiting_label, LV_ALIGN_CENTER, 0, 10);
}

void app_register_m5stack_display_event_handler()
{
    esp_event_handler_register(APP_NETWORK_EVENT, APP_NETWORK_EVENT_QR_DISPLAY, qr_display_event_handler, NULL);
    esp_event_handler_register(OPENTHREAD_EVENT, OPENTHREAD_EVENT_ROLE_CHANGED, qr_display_event_handler, NULL);
    esp_event_handler_register(OPENTHREAD_EVENT, OPENTHREAD_EVENT_DATASET_CHANGED, qr_display_event_handler, NULL);
}

#endif /* CONFIG_M5STACK_THREAD_BR_BOARD */
