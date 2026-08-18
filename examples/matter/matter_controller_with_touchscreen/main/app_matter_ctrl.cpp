/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <app_matter_ctrl.h>
#include <app_matter_device_list.h>
#include <app_matter_view_model.h>
#include <devices/onoff.h>
#include <ui_matter_ctrl.h>

#include <app_controller.h>
#include <app_rmaker_matter_report.h>
#include <app_rmaker_matter_device_list.h>
#include <esp_check.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>

#include <string.h>

static const char *TAG = "app_matter_ctrl";
static char s_qr_payload[160];
static bool s_is_provisioned;
static TaskHandle_t s_refresh_ui_task_handle;

static void matter_ctrl_refresh_ui(void)
{
    matter_vm_notify_refresh();
}

void matter_ctrl_primary_action(uint64_t node_id, uint16_t endpoint_id)
{
    matter_device_vm_item_t item = {};
    if (!matter_vm_get_device(node_id, endpoint_id, &item) || !item.is_online) {
        return;
    }

    if (matter_device_type_is_onoff(item.device_type)) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(matter_onoff_primary_action(node_id, endpoint_id));
    }
}

const char *matter_ctrl_get_qr_payload(void)
{
    return s_qr_payload;
}

void matter_ctrl_set_qr_payload(const char *payload)
{
    if (!payload) {
        s_qr_payload[0] = 0;
        return;
    }
    strlcpy(s_qr_payload, payload, sizeof(s_qr_payload));
    matter_vm_notify_loading();
}

void matter_ctrl_set_provisioned(bool provisioned)
{
    s_is_provisioned = provisioned;
    matter_vm_notify_refresh();
}

bool matter_ctrl_is_provisioned(void)
{
    return s_is_provisioned;
}

void matter_ctl_on_matter_report(const app_rmaker_matter_report_t *report, void *priv_data)
{
    (void)priv_data;
    if (!report) {
        return;
    }

    if (report->type == APP_RMAKER_MATTER_REPORT_ONLINE) {
        cJSON *online = cJSON_GetObjectItem(report->data, "online");
        if (cJSON_IsBool(online) && matter_device_list_set_node_online(report->node_id, cJSON_IsTrue(online))) {
            matter_vm_notify_refresh();
        }
        return;
    }

    if (report->type == APP_RMAKER_MATTER_REPORT_ATTR) {
        matter_onoff_on_attr_report(report);
    }
}

void matter_ctrl_on_device_list_update(esp_err_t err, const matter_device_t *dev_list)
{
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Device list update failed: %s", esp_err_to_name(err));
        matter_device_t *fallback = app_controller_device_list_copy();
        if (fallback) {
            ESP_LOGI(TAG, "Rebuilding UI from previous good Matter device list");
            matter_device_list_rebuild(fallback);
            app_rmaker_device_list_copy_destroy(fallback);
        }
    } else {
        matter_device_list_rebuild(dev_list);
    }

    if (s_refresh_ui_task_handle) {
        xTaskNotifyGive(s_refresh_ui_task_handle);
    } else {
        matter_ctrl_refresh_ui();
    }
}

static void refresh_ui_task(void *pvParameters)
{
    (void)pvParameters;

    while (true) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == true) {
            matter_ctrl_refresh_ui();
        }
    }
}

esp_err_t matter_ctrl_ui_init(void)
{
    BaseType_t ret = xTaskCreatePinnedToCoreWithCaps(refresh_ui_task, "refresh_ui", 4096, NULL, tskIDLE_PRIORITY,
                                                     &s_refresh_ui_task_handle, 1, MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(ret == pdPASS, ESP_FAIL, TAG, "Failed to create refresh UI task");
    return ESP_OK;
}
