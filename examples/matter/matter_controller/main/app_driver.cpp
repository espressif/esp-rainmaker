/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include <app_matter.h>
#include <app_priv.h>
#include <device.h>
#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_controller_cluster_command.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <matter_controller_device_mgr.h>

using namespace esp_matter;
using namespace chip::app::Clusters;
using esp_matter::controller::device_mgr::matter_device_t;

static const char* TAG = "app_driver";
extern uint16_t app_endpoint_id;
static matter_device_t* s_device_list = NULL;
static matter_device_t* s_selected_device = NULL;

void on_device_list_update()
{
    if (s_device_list) {
        esp_matter::controller::device_mgr::free_device_list(s_device_list);
    }
    s_device_list = esp_matter::controller::device_mgr::get_device_list_clone();
    s_selected_device = s_device_list;
}

static void send_toggle_command(intptr_t context)
{
    if (s_selected_device) {
        esp_matter::controller::send_invoke_cluster_command(s_selected_device->node_id, s_selected_device->endpoints[0].endpoint_id,
                                                            OnOff::Id, OnOff::Commands::Toggle::Id, NULL);
    }
}

static void app_driver_button_single_click_cb(void* handle, void* usr_data)
{
    ESP_LOGI(TAG, "button single click");
    chip::DeviceLayer::PlatformMgr().ScheduleWork(send_toggle_command, reinterpret_cast<intptr_t>(NULL));
}

static void app_driver_button_double_click_cb(void* handle, void* user_data)
{
    ESP_LOGI(TAG, "button double click");
    if (!s_selected_device || !s_selected_device->next) {
        s_selected_device = s_device_list;
    } else {
        s_selected_device = s_selected_device->next;
    }

    if (s_selected_device) {
        ESP_LOGI(TAG, "switch to next device, node id: 0x%llx", s_selected_device->node_id);
    }
}

app_driver_handle_t app_driver_button_init(void* user_data)
{
    /* Initialize button */
    button_config_t config = button_driver_get_config();
    button_handle_t handle = iot_button_create(&config);
    iot_button_register_cb(handle, BUTTON_SINGLE_CLICK, app_driver_button_single_click_cb, user_data);
    iot_button_register_cb(handle, BUTTON_DOUBLE_CLICK, app_driver_button_double_click_cb, user_data);
    return (app_driver_handle_t)handle;
}
