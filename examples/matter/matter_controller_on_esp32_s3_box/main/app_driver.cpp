/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include <app/server/Server.h>
#include <app_matter.h>
#include <app_priv.h>
#include <device.h>
#include <esp_check.h>
#include <esp_matter.h>
#include <esp_matter_controller_cluster_command.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <led_driver.h>
#include <matter_controller_device_mgr.h>
#include <system/SystemClock.h>
#include <system/SystemLayerImplFreeRTOS.h>

#include "app_matter_ctrl.h"
#include "ui_matter_ctrl.h"

using namespace esp_matter;
using namespace chip::app::Clusters;
using esp_matter::controller::device_mgr::endpoint_entry_t;
using esp_matter::controller::device_mgr::matter_device_t;

static const char *TAG = "app_driver";
static bool device_get_flag = false;
static uint16_t device_update_timer = 40;
static uint64_t device_node_id = 0;
static matter_device_t *s_device_ptr = NULL;
TaskHandle_t xRefresh_Ui_Handle = NULL;

typedef struct endpoint_type {
    uint16_t endpoint_id;
    struct endpoint_type *next;
} endpoint_type_t;

/* Be called after updating device list */
void on_device_list_update(void)
{
    if (s_device_ptr) {
        esp_matter::controller::device_mgr::free_device_list(s_device_ptr);
    }
    s_device_ptr = esp_matter::controller::device_mgr::get_device_list_clone();
    uint64_t node_id = 0;
    matter_device_t *ptr = s_device_ptr;

    if (!s_device_ptr) {
        ESP_LOGE(TAG, "No device list");
        device_get_flag = true;
        esp_matter::controller::device_mgr::free_device_list(s_device_ptr);
        /* After removing the only one device, invoking matter_ctrl_get_device to update */
        matter_ctrl_get_device((void *)s_device_ptr);
        if (xRefresh_Ui_Handle) {
            xTaskNotifyGive(xRefresh_Ui_Handle);
        }
        return;
    }
    int dev_count=0;
    while (ptr) {
        if (ptr->reachable) {
            node_id += ptr->node_id;
        } else {
            node_id -= ptr->node_id;
        }
        ptr = ptr->next;
        dev_count++;
    }

    if (device_get_flag) {
        /* device list no change */
        if (device_node_id == node_id) {
            ESP_LOGI(TAG, "device list no change");
        } else {
            /* device list has changed, reestablishing the device-list */
            matter_ctrl_get_device((void *)s_device_ptr);
            matter_ctrl_subscribe_device_state(SUBSCRIBE_LOCAL_DEVICE);
            if (xRefresh_Ui_Handle) {
                xTaskNotifyGive(xRefresh_Ui_Handle);
            }
            ESP_LOGI(TAG, "update device list successfully");
        }
    } else {
        /* don't refresh at the first time */
        matter_ctrl_get_device((void *)s_device_ptr);
        matter_ctrl_subscribe_device_state(SUBSCRIBE_LOCAL_DEVICE);
        device_get_flag = true;
        if (xRefresh_Ui_Handle) {
            xTaskNotifyGive(xRefresh_Ui_Handle);
        }
    }
    device_node_id = node_id;
    esp_matter::controller::device_mgr::free_device_list(s_device_ptr);
    s_device_ptr = NULL;
    ESP_LOGI(TAG,"\ngot %d devices from cloud\n",dev_count);
    read_dev_info();
}

app_driver_handle_t app_driver_button_init(void *user_data)
{
    /* Initialize button */
    button_config_t config = button_driver_get_config();
    button_handle_t handle = iot_button_create(&config);
    return (app_driver_handle_t)handle;
}

static void refresh_ui_task(void *pvParameters)
{
    while (true) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == true) {
            /* refresh ui */
            clean_screen_with_button();
            ui_matter_config_update_cb(UI_MATTER_EVT_REFRESH);
        }
    }
}

static void Layer_timer_cb(chip::System::Layer *aLayer, void *appState)
{
    if (device_get_flag) {
        esp_matter::controller::device_mgr::update_device_list(0);
        matter_ctrl_subscribe_device_state(SUBSCRIBE_LOCAL_DEVICE);
    }
    esp_matter::lock::chip_stack_lock(portMAX_DELAY);
    CHIP_ERROR chip_err = chip::DeviceLayer::SystemLayer().StartTimer(
        chip::System::Clock::Seconds32(device_update_timer), Layer_timer_cb, nullptr);
    esp_matter::lock::chip_stack_unlock();
    if (chip_err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "update timer start failed");
    }
}

esp_err_t update_device_refresh_ui_init()
{
    esp_err_t esp_log = ESP_OK;
    esp_matter::lock::chip_stack_lock(portMAX_DELAY);
    CHIP_ERROR chip_err = chip::DeviceLayer::SystemLayer().StartTimer(
        chip::System::Clock::Seconds32(device_update_timer), Layer_timer_cb, nullptr);
    esp_matter::lock::chip_stack_unlock();
    if (chip_err != CHIP_NO_ERROR) {
        esp_log = ESP_FAIL;
        ESP_LOGE(TAG, "update timer start failed!");
    }

    xTaskCreatePinnedToCore(refresh_ui_task, "refresh_ui", 4096, nullptr, tskIDLE_PRIORITY, &xRefresh_Ui_Handle, 1);
    if (xRefresh_Ui_Handle == NULL) {
        ESP_LOGE(TAG, "creat task for refresh ui failed!");
    }
    configASSERT(xRefresh_Ui_Handle);
    return esp_log;
}