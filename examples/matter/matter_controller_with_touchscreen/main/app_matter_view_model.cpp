#include <app_matter_ctrl.h>
#include <app_matter_device_list.h>
#include <app_matter_view_model.h>
#include <app_rmaker_matter_device_list.h>
#include <ui_matter_ctrl.h>

#include <string.h>

bool matter_vm_get_status(matter_device_vm_status_t *out)
{
    if (!out) {
        return false;
    }
    matter_device_list_lock();
    out->device_count = matter_device_list.device_num;
    out->online_count = matter_device_list.online_num;
    out->fetchable = app_rmaker_matter_device_list_updatable();
    matter_device_list_unlock();
    return true;
}

bool matter_vm_copy_devices(matter_device_vm_item_t *out, size_t max_count, size_t *out_count)
{
    if (out_count) {
        *out_count = 0;
    }
    if (!out || !out_count || max_count == 0) {
        return false;
    }

    size_t count = 0;
    matter_device_list_lock();
    for (matter_device_list_node_t *node = matter_device_list.dev_list; node && count < max_count; node = node->next) {
        matter_device_vm_item_t *item = &out[count++];
        item->node_id = node->node_id;
        item->endpoint_id = node->endpoint_id;
        item->device_type = node->device_type;
        strlcpy(item->name, node->name, sizeof(item->name));
        item->is_online = node->is_online;
        item->state = node->state;
    }
    matter_device_list_unlock();
    *out_count = count;
    return true;
}

bool matter_vm_get_device(uint64_t node_id, uint16_t endpoint_id, matter_device_vm_item_t *out)
{
    if (!out) {
        return false;
    }
    bool found = false;
    matter_device_list_lock();
    matter_device_list_node_t *node = matter_device_list_find_locked(node_id, endpoint_id);
    if (node) {
        out->node_id = node->node_id;
        out->endpoint_id = node->endpoint_id;
        out->device_type = node->device_type;
        strlcpy(out->name, node->name, sizeof(out->name));
        out->is_online = node->is_online;
        out->state = node->state;
        found = true;
    }
    matter_device_list_unlock();
    return found;
}

void matter_vm_notify_refresh(void)
{
    ui_matter_config_update_cb(matter_ctrl_is_provisioned() ? UI_MATTER_EVT_REFRESH : UI_MATTER_EVT_LOADING);
}

void matter_vm_notify_loading(void)
{
    ui_matter_config_update_cb(UI_MATTER_EVT_LOADING);
}
