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

    if(ptr->cmd_data)
        ESP_LOGI(TAG,"\ncmd_data: %s\n",ptr->cmd_data);

    if (ptr) {
        ESP_LOGI(TAG, "send command to node %llx endpoint %d cluster %d command %d", ptr->node_id, ptr->endpoint_id, ptr->cluster_id, ptr->command_id);
        esp_matter::controller::send_invoke_cluster_command(ptr->node_id, ptr->endpoint_id, ptr->cluster_id, ptr->command_id, ptr->cmd_data);
    }
    else
        ESP_LOGE(TAG, "send command with null ptr");

    delete ptr;
}

CHIP_ERROR send_command(intptr_t arg)
{
    return chip::DeviceLayer::PlatformMgr().ScheduleWork(send_command_cb, arg);
}