/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "m5display.h"
#include "nwk/esp_zigbee_nwk.h"
#include "esp_app_rainmaker.h"

#if CONFIG_M5STACK_ZIGBEE_GATEWAY_BOARD
#include <app_insights.h>
#include <app_wifi.h>
#include <bsp/esp-bsp.h>
#include <esp_openthread_lock.h>
#include <esp_rmaker_thread_br.h>
#include <esp_rmaker_utils.h>
#include <esp_wifi.h>
#include <lvgl.h>

ESP_EVENT_DEFINE_BASE(M5STACK_DISPLAY_EVENT);
static const char *TAG = "m5_display";
static lv_obj_t *page = NULL;
static lv_obj_t *qrcode = NULL;
static lv_obj_t *tips_label = NULL;
static lv_obj_t *waiting_label = NULL;
static lv_obj_t *wifi_ssid_text = NULL;
static lv_obj_t *zigbee_info_text = NULL;
static lv_obj_t *zigbee_img = NULL;
static lv_obj_t *add_device_switch = NULL;
static lv_obj_t *add_device_text = NULL;
static char *qr_data = NULL;
TaskHandle_t xRefresh_Ui_Handle = NULL;
LV_IMG_DECLARE(zigbee)
LV_IMG_DECLARE(logo)
#define LV_SYMBOL_EXTRA_SYNC "\xef\x80\xA1"

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
        lv_msgbox_add_text(msgbox, "Are you sure to reset Zigbee Gateway?");
        lv_obj_t *btn = lv_msgbox_add_footer_button(msgbox, "Ok");
        lv_obj_set_size(btn, 80, 40);
        lv_obj_add_event_cb(btn, msg_btn_event_cb, LV_EVENT_CLICKED, NULL);
        btn = lv_msgbox_add_footer_button(msgbox, "Cancel");
        lv_obj_set_size(btn, 80, 40);
        lv_obj_add_event_cb(btn, msg_btn_event_cb, LV_EVENT_CLICKED, NULL);
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

#if !CONFIG_ZIGBEE_INSTALLCODE_ENABLED
static void switch_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        LV_UNUSED(obj);
        bool add_device_flag = lv_obj_has_state(add_device_switch, LV_STATE_CHECKED);
        esp_event_post(M5STACK_DISPLAY_EVENT, DISPLAY_ZIGBEE_ADD_DEVICE_STATE, &add_device_flag, sizeof(add_device_flag), portMAX_DELAY);
        if (add_device_flag) {
            esp_app_enable_zigbee_add_device();
        } else {
            esp_app_disable_zigbee_add_device();
        }
        esp_rainmaker_report_zigbee_add_device_state(add_device_flag);
    }
}
#endif

static void display_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    bsp_display_lock(0);
    if (waiting_label && !lv_obj_has_flag(waiting_label, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(waiting_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (event_base == APP_NETWORK_EVENT && event_id == APP_NETWORK_EVENT_QR_DISPLAY) {
        qr_data = (char *)event_data;
        qrcode = lv_qrcode_create(page);
        lv_obj_align(qrcode, LV_ALIGN_CENTER, 0, 10);
        lv_qrcode_update(qrcode, qr_data, strlen(qr_data));
        tips_label = lv_label_create(page);
        lv_label_set_text_static(tips_label, "Use RainMaker to set Zigbee Gateway");
        lv_obj_align_to(tips_label, qrcode, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    } else if (event_base == APP_NETWORK_EVENT && event_id == APP_NETWORK_EVENT_PROV_TIMEOUT) {
        if (tips_label) {
            lv_label_set_text_static(tips_label, "Provision timeout. Please reboot");
        }
    } else if (event_base == APP_NETWORK_EVENT && event_id == APP_NETWORK_EVENT_PROV_CRED_MISMATCH) {
        if (tips_label) {
            lv_label_set_text_static(tips_label, "WiFi credential mismatch");
        }
    } else if (event_base == M5STACK_DISPLAY_EVENT && event_id == DISPLAY_ZIGBEE_PANID_EVENT) {
        if (!zigbee_img) {
            zigbee_img = lv_img_create(page);
            zigbee_info_text = lv_label_create(page);
            wifi_ssid_text = lv_label_create(page);

#if !CONFIG_ZIGBEE_INSTALLCODE_ENABLED
            add_device_switch = lv_switch_create(page);
            static lv_style_t style_switch;
            lv_style_init(&style_switch);
            lv_style_set_width(&style_switch, 80);
            lv_style_set_height(&style_switch, 40);
            lv_obj_add_style(add_device_switch, &style_switch, LV_STATE_DEFAULT);
            add_device_text = lv_label_create(page);
#endif
        }
        lv_obj_align(zigbee_img, LV_ALIGN_LEFT_MID, 0, 0);
        lv_img_set_src(zigbee_img, &zigbee);

        wifi_config_t sta_config;
        esp_wifi_get_config(WIFI_IF_STA, &sta_config);
        lv_label_set_text_fmt(wifi_ssid_text, "wifi ssid: %s", sta_config.sta.ssid);
        lv_obj_align_to(wifi_ssid_text, zigbee_img, LV_ALIGN_OUT_RIGHT_TOP, 10, 20);

        lv_label_set_text_fmt(zigbee_info_text, "panid: 0x%4x\nchannel: %d\ndevice count: %d\n", esp_zb_get_pan_id(), esp_zb_get_current_channel(), get_zigbee_device_count());
        lv_obj_align_to(zigbee_info_text, wifi_ssid_text, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

#if !CONFIG_ZIGBEE_INSTALLCODE_ENABLED
        lv_obj_add_state(add_device_switch, LV_STATE_CHECKED);
        lv_obj_set_state(add_device_switch, LV_STATE_CHECKED, false);
        lv_obj_add_event_cb(add_device_switch, switch_event_handler, LV_EVENT_ALL, NULL);
        lv_obj_align_to(add_device_switch, zigbee_img, LV_ALIGN_OUT_BOTTOM_LEFT, 10, 10);

        lv_label_set_text_fmt(add_device_text, "Add zigbee device:%s", lv_obj_has_state(add_device_switch, LV_STATE_CHECKED) ? "ON" : "OFF");
        lv_obj_align_to(add_device_text, add_device_switch, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
#endif

        if (qrcode) {
            lv_obj_add_flag(qrcode, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(tips_label, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (event_base == M5STACK_DISPLAY_EVENT && event_id == DISPLAY_ZIGBEE_DEVICE_COUNT_EVENT) {
        if (zigbee_info_text) {
            uint8_t device_count = *(uint8_t *)event_data;
            lv_label_set_text_fmt(zigbee_info_text, "panid: 0x%4x\nchannel: %d\ndevice count: %d\n", esp_zb_get_pan_id(), esp_zb_get_current_channel(), device_count);
        }
    } else if (event_base == M5STACK_DISPLAY_EVENT && event_id == DISPLAY_ZIGBEE_ADD_DEVICE_STATE) {
#if !CONFIG_ZIGBEE_INSTALLCODE_ENABLED
        if (add_device_text) {
            bool add_device_flag = *(bool *)event_data;
            if (add_device_text) {
                lv_label_set_text_fmt(add_device_text, "Add zigbee device: %s", add_device_flag ? "ON" : "OFF");
                if (add_device_flag != lv_obj_has_state(add_device_switch, LV_STATE_CHECKED)) {
                    lv_obj_set_state(add_device_switch, LV_STATE_CHECKED, add_device_flag);
                }
            }
        }
#endif
    }
    bsp_display_unlock();
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
    lv_label_set_text_static(waiting_label, "Waiting for device initialization");
    lv_obj_align(waiting_label, LV_ALIGN_CENTER, 0, 10);
}

void app_register_m5stack_display_event_handler()
{
    esp_event_handler_register(APP_NETWORK_EVENT, ESP_EVENT_ANY_ID, display_event_handler, NULL);
    esp_event_handler_register(M5STACK_DISPLAY_EVENT, ESP_EVENT_ANY_ID, display_event_handler, NULL);
}

#endif /* CONFIG_M5STACK_ZIGBEE_GATEWAY_BOARD */
