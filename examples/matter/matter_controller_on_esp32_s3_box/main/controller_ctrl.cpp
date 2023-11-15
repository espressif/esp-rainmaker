#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <string.h>
#include <led_driver.h>
#include <esp_matter_rainmaker.h>
#include <platform/ESP32/route_hook/ESP32RouteHook.h>
#include <esp_matter_console.h>
#include <app_matter.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_core.h>
#include <app_priv.h>
#include <matter_controller_cluster.h>
#include <esp_matter_controller_console.h>
#include <matter_controller_device_mgr.h>
#include <app/server/Server.h>
#include <iostream>
#include <esp_matter_controller_cluster_command.h>
#include <controller_ctrl.h>

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::cluster;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "controller-ctrl";


static void send_command_cb(intptr_t arg) 
{
    send_cmd_format*ptr = (send_cmd_format *)arg;
    // const char *cmd_data[] = { "0x6"/* on-off-cluster*/, "0x2" /* toggle */};

    const char* cmd_data_cstr[ptr->cmd_data.size()];

    for (size_t i = 0; i < ptr->cmd_data.size(); ++i) 
    {
        cmd_data_cstr[i] = ptr->cmd_data[i].c_str();
        ESP_LOGI(TAG,"\ndata: %d : %s\n",i,cmd_data_cstr[i]);
    }
    
    if (ptr) {
        ESP_LOGI(TAG, "send command to node %llx endpoint %d", ptr->node_id, ptr->endpoint_id);
        esp_matter::controller::send_invoke_cluster_command(ptr->node_id, ptr->endpoint_id, ptr->cmd_data.size(), (char **)cmd_data_cstr);
    }
    else
        ESP_LOGE(TAG, "send command with null ptr");

    ptr->cmd_data.clear();
    free(ptr);
}

void send_command(intptr_t arg)
{
    chip::DeviceLayer::PlatformMgr().ScheduleWork(send_command_cb, arg);
}