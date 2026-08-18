#include "devices/onoff.h"

#include <app_matter_view_model.h>
#include <esp_check.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_matter_controller_cluster_command.h>
#include <ui_matter_ctrl.h>

#include <esp_matter.h>
#include <platform/PlatformManager.h>

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace chip::app::Clusters;

static const char *TAG = "matter_onoff";

typedef struct {
    uint64_t node_id;
    uint16_t endpoint_id;
    bool onoff;
} onoff_target_t;

static bool parse_unsigned_key(const char *key, int base, uint64_t max_value, uint64_t *value)
{
    if (!key || !*key || !value) {
        return false;
    }
    char *end = NULL;
    unsigned long long parsed = strtoull(key, &end, base);
    if (!end || *end != '\0' || parsed > max_value) {
        return false;
    }
    *value = parsed;
    return true;
}

void matter_onoff_on_attr_report(const app_rmaker_matter_report_t *report)
{
    if (!report || report->type != APP_RMAKER_MATTER_REPORT_ATTR || !cJSON_IsObject(report->data)) {
        return;
    }

    bool rebuild_needed = false;
    cJSON *node_obj = NULL;
    cJSON_ArrayForEach(node_obj, report->data) {
        uint64_t node_id = 0;
        if (!cJSON_IsObject(node_obj) || !parse_unsigned_key(node_obj->string, 16, UINT64_MAX, &node_id)) {
            continue;
        }
        cJSON *ep_obj = NULL;
        cJSON_ArrayForEach(ep_obj, node_obj) {
            uint64_t endpoint_id = 0;
            if (!cJSON_IsObject(ep_obj) || !parse_unsigned_key(ep_obj->string, 0, UINT16_MAX, &endpoint_id)) {
                continue;
            }
            char cluster_key[24];
            char attr_key[24];
            snprintf(cluster_key, sizeof(cluster_key), "0x%" PRIX32, OnOff::Id);
            snprintf(attr_key, sizeof(attr_key), "0x%" PRIX32, OnOff::Attributes::OnOff::Id);
            cJSON *cluster_obj = cJSON_GetObjectItem(ep_obj, cluster_key);
            cJSON *attr_obj = cluster_obj ? cJSON_GetObjectItem(cluster_obj, attr_key) : NULL;
            cJSON *value = attr_obj ? cJSON_GetObjectItem(attr_obj, "value") : NULL;
            if (!cJSON_IsBool(value)) {
                continue;
            }

            bool online_changed = false;
            bool state_changed = false;
            matter_device_list_lock();
            matter_device_list_node_t *node = matter_device_list_find_locked(node_id, endpoint_id);
            if (node && matter_device_type_is_onoff(node->device_type)) {
                online_changed = matter_device_list_set_online_locked(node, true);
                bool onoff = cJSON_IsTrue(value);
                if (node->state.onoff.onoff != onoff) {
                    node->state.onoff.onoff = onoff;
                    state_changed = true;
                }
            }
            matter_device_list_unlock();

            if (online_changed) {
                rebuild_needed = true;
            } else if (state_changed) {
                ui_matter_device_state_update(node_id, endpoint_id);
            }
        }
    }

    if (rebuild_needed) {
        matter_vm_notify_refresh();
    }
}

static void set_onoff_work(intptr_t arg)
{
    onoff_target_t *target = (onoff_target_t *)arg;
    ESP_RETURN_VOID_ON_FALSE(target, TAG, "OnOff target is NULL");

    auto on_success = [](void *, const chip::app::ConcreteCommandPath &, const chip::app::StatusIB & status,
    chip::TLV::TLVReader *) {
        if (!status.IsSuccess()) {
            ESP_LOGW(TAG, "OnOff command returned failure status");
        }
    };
    auto on_error = [](void *, CHIP_ERROR error) {
        ESP_LOGW(TAG, "OnOff command failed: %s", chip::ErrorStr(error));
    };

    const uint32_t command_id = target->onoff ? OnOff::Commands::On::Id : OnOff::Commands::Off::Id;
    auto *cmd = chip::Platform::New<esp_matter::controller::cluster_command>(
                    target->node_id, target->endpoint_id, OnOff::Id, command_id, nullptr, chip::NullOptional, on_success, on_error);
    if (cmd) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(cmd->send_command());
    } else {
        ESP_LOGE(TAG, "Failed to allocate OnOff command");
    }
    free(target);
}

esp_err_t matter_onoff_primary_action(uint64_t node_id, uint16_t endpoint_id)
{
    bool desired = false;
    bool changed = false;
    esp_err_t ret = ESP_OK;

    matter_device_list_lock();
    matter_device_list_node_t *node = matter_device_list_find_locked(node_id, endpoint_id);
    if (!node || !node->is_online || !matter_device_type_is_onoff(node->device_type)) {
        ESP_LOGW(TAG, "OnOff action target is not controllable");
        ret = ESP_ERR_INVALID_STATE;
    } else {
        desired = !node->state.onoff.onoff;
        node->state.onoff.onoff = desired;
        changed = true;
    }

    matter_device_list_unlock();
    ESP_RETURN_ON_ERROR(ret, TAG, "Invalid OnOff target");
    if (changed) {
        ui_matter_device_state_update(node_id, endpoint_id);
    }
    onoff_target_t *target = (onoff_target_t *)heap_caps_calloc_prefer(1, sizeof(onoff_target_t), 2,
                                                                       MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM,
                                                                       MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
    ESP_RETURN_ON_FALSE(target, ESP_ERR_NO_MEM, TAG, "Failed to allocate OnOff target");
    target->node_id = node_id;
    target->endpoint_id = endpoint_id;
    target->onoff = desired;

    CHIP_ERROR err = chip::DeviceLayer::PlatformMgr().ScheduleWork(set_onoff_work, (intptr_t)target);
    if (err != CHIP_NO_ERROR) {
        free(target);
        ESP_LOGW(TAG, "Failed to schedule OnOff command: %s", chip::ErrorStr(err));
        return ESP_FAIL;
    }
    return ESP_OK;
}
