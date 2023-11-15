/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include <DataModelLogger.h>
#include <app_matter.h>
#include <device.h>
#include <esp_check.h>
#include <esp_matter.h>
#include <esp_matter_controller_cluster_command.h>
#include <esp_matter_controller_read_command.h>
#include <esp_matter_controller_subscribe_command.h>
#include <esp_matter_core.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <led_driver.h>
#include <matter_controller_device_mgr.h>

#include "app_matter_ctrl.h"
#include "ui_matter_ctrl.h"

#include <read_node_info.h>

/* maintain a local subscribe list for Matter only device */
typedef struct local_device_subscribe_list {
    esp_matter::controller::subscribe_command *subscribe_ptr;
    bool is_searched;
    bool is_subscribed;
    uint8_t offline_time;
    uint16_t endpoint_id;
    uint64_t node_id;
    struct local_device_subscribe_list *next;
} local_device_subscribe_list_t;

using namespace esp_matter;
using namespace esp_matter::controller;
using namespace chip;
using namespace chip::app::Clusters;
using esp_matter::controller::device_mgr::endpoint_entry_t;
using esp_matter::controller::device_mgr::matter_device_t;

static void _subscribe_local_device_state(intptr_t arg);
static uint32_t cluster_id = 0x6;
static uint32_t attribute_id = 0x0;
static uint32_t min_interval = 0;
static uint32_t max_interval = 20;
static SemaphoreHandle_t device_list_mutex = NULL;
static local_device_subscribe_list_t *local_subscribe_list = NULL;
static const char *TAG = "app_matter_ctrl";
static matter_device_t *m_device_ptr = NULL;
device_to_control_t device_to_control = {0, 0, NULL};
extern TaskHandle_t xRefresh_Ui_Handle;

class device_list_lock {
public:
    device_list_lock()
    {
        if (device_list_mutex) {
            xSemaphoreTake(device_list_mutex, portMAX_DELAY);
        }
    }

    ~device_list_lock()
    {
        if (device_list_mutex) {
            xSemaphoreGive(device_list_mutex);
        }
    }
};

static node_endpoint_id_list_t *get_tail_device(node_endpoint_id_list_t *dev_list)
{
    if (!dev_list)
        return NULL;
    node_endpoint_id_list_t *tail = dev_list;
    while (tail->next) {
        tail = tail->next;
    }
    return tail;
}

/* be called when subscribe successfully */
static void attribute_data_cb(uint64_t remote_node_id, const chip::app::ConcreteDataAttributePath &path,
                              chip::TLV::TLVReader *data)
{
    ESP_LOGI(TAG, "attribute subscribe  callback");
    switch (path.mClusterId) {
    case OnOff::Id: {
        switch (path.mAttributeId) {
        case OnOff::Attributes::OnOff::Id: {
            bool value;
            chip::app::DataModel::Decode(*data, value);

            /* for online device, refresh the device on/off state */
            device_list_lock my_device_lock;
            node_endpoint_id_list_t *dev_ptr = device_to_control.dev_list;
            while (dev_ptr) {
                if (dev_ptr->node_id == remote_node_id && dev_ptr->endpoint_id == path.mEndpointId) {
                    /* For local device, if it's offline, change state */
                    if (!dev_ptr->is_Rainmaker_device && !dev_ptr->is_online) {
                        dev_ptr->OnOff = value;
                        ++device_to_control.online_num;
                        dev_ptr->is_online = true;
                        local_device_subscribe_list_t *s_ptr = local_subscribe_list;
                        while (s_ptr) {
                            if (s_ptr->node_id == dev_ptr->node_id && s_ptr->endpoint_id == dev_ptr->endpoint_id) {
                                s_ptr->offline_time = 0u;
                                s_ptr->is_subscribed = true;
                                break;
                            }
                            s_ptr = s_ptr->next;
                        }
                        ESP_LOGI(TAG, "local device comes online");
                        if (xRefresh_Ui_Handle) {
                            xTaskNotifyGive(xRefresh_Ui_Handle);
                        }
                        return;
                    }

                    if (dev_ptr->OnOff == value) {
                        ESP_LOGI(TAG, "%llx OnOff attribute no change", remote_node_id);
                        std::string update_val = std::to_string(value);
                        change_data_model_attribute(remote_node_id, path.mEndpointId,path.mClusterId, path.mAttributeId,update_val);
                        return;
                    }

                    dev_ptr->OnOff = value;
                    ESP_LOGE(TAG, "%llx OnOff attribute change %d", remote_node_id, value);
                    std::string update_val = std::to_string(value);
                    change_data_model_attribute(remote_node_id, path.mEndpointId,path.mClusterId, path.mAttributeId,update_val);
                    if (xRefresh_Ui_Handle) {
                        xTaskNotifyGive(xRefresh_Ui_Handle);
                    }
                    return;
                }
                dev_ptr = dev_ptr->next;
            }
        }
        default:
            break;
        }
    }
    default:
        break;
    }
    return;
}

/* be called when subscribe connecting failed */
static void subscribe_failed_cb(void *subscribe_cmd)
{
    ESP_LOGI(TAG, "subscribe connecting failed callback");
    esp_matter::controller::subscribe_command *sub_cmd = (subscribe_command *)subscribe_cmd;
    device_list_lock my_device_lock;

    /* the subscribe command will be removed automatically after connecting failed, set the ptr in local device
     * subscribe list to null */
    local_device_subscribe_list_t *sub_ptr = local_subscribe_list;
    while (sub_ptr) {
        if (sub_ptr->subscribe_ptr == sub_cmd) {
            /* set the on/offline state*/
            node_endpoint_id_list_t *dev_ptr = device_to_control.dev_list;
            while (dev_ptr) {
                if (!dev_ptr->is_Rainmaker_device && dev_ptr->node_id == sub_ptr->node_id &&
                    dev_ptr->endpoint_id == sub_ptr->endpoint_id && dev_ptr->is_online) {
                    --device_to_control.online_num;
                    dev_ptr->is_online = false;
                    if (xRefresh_Ui_Handle) {
                        xTaskNotifyGive(xRefresh_Ui_Handle);
                    }
                    break;
                } else if (dev_ptr->is_Rainmaker_device && dev_ptr->node_id == sub_ptr->node_id &&
                           dev_ptr->endpoint_id == sub_ptr->endpoint_id) {
                    ESP_LOGE(TAG, "Rainmaker device %llx subscribe connecting failed", sub_ptr->node_id);
                    dev_ptr->is_subscribed = false;
                    break;
                }
                dev_ptr = dev_ptr->next;
            }

            sub_ptr->offline_time = 0;
            sub_ptr->subscribe_ptr = NULL;
            ESP_LOGI(TAG, "SET subscribe ptr of node %llx to null", sub_ptr->node_id);
            break;
        }
        sub_ptr = sub_ptr->next;
    }
}

/* be called when subscribe timeout */
static void subscribe_done_cb(uint64_t remote_node_id, uint32_t subscription_id)
{
    ESP_LOGE(TAG, "subscribe done callback");
    device_list_lock my_device_lock;
    node_endpoint_id_list_t *ptr = device_to_control.dev_list;
    while (ptr) {
        /* for rainmaker device, on/offline is not subscribed, but sometimes subscribe_done happens */
        if (ptr->node_id == remote_node_id) {
            if (ptr->is_Rainmaker_device) {
                ESP_LOGE(TAG, "Rainmaker device %llx subscribe done", remote_node_id);
                ptr->is_subscribed = false;
                break;
            }

            /* set the ptr in local device subscribe list to null */
            local_device_subscribe_list_t *sub_ptr = local_subscribe_list;
            while (sub_ptr) {
                if (sub_ptr->node_id == remote_node_id) {
                    /* the memory allocated for subscribe command is freed by chip::Platform::Delete() automaticaly */
                    ++sub_ptr->offline_time;
                    sub_ptr->subscribe_ptr = NULL;
                    sub_ptr->is_subscribed = false;

                    if (!ptr->is_Rainmaker_device && ptr->is_online) {
                        /* after subscribe timeout, re-send subscribe to check state */
                        sub_ptr->subscribe_ptr = chip::Platform::New<subscribe_command>(
                            sub_ptr->node_id, sub_ptr->endpoint_id, cluster_id, attribute_id, SUBSCRIBE_ATTRIBUTE,
                            min_interval, max_interval,true, attribute_data_cb, nullptr, subscribe_done_cb,
                            subscribe_failed_cb);

                        if (!sub_ptr->subscribe_ptr) {
                            ESP_LOGE(TAG, "Failed to alloc memory for subscribe-local-device command");
                            return;
                        }
                        chip::DeviceLayer::PlatformMgr().ScheduleWork(_subscribe_local_device_state, (intptr_t)sub_ptr);
                    }
                    ESP_LOGE(TAG, "device %llx subscribe done", remote_node_id);
                    break;
                }
                sub_ptr = sub_ptr->next;
            }
        }
        ptr = ptr->next;
    }
}

static void _subscribe_local_device_state(intptr_t arg)
{
    local_device_subscribe_list_t *ptr = (local_device_subscribe_list_t *)arg;
    if (!ptr || !ptr->subscribe_ptr) {
        ESP_LOGE(TAG, "Subscribe device state with null ptr");
        return;
    }

    ptr->subscribe_ptr->send_command();
}

static void _subscribe_rainmaker_device_state(intptr_t arg)
{
    node_endpoint_id_list_t *ptr = (node_endpoint_id_list_t *)arg;
    if (!ptr) {
        ESP_LOGE(TAG, "Subscribe device state with null ptr");
        return;
    }

    esp_matter::controller::subscribe_command *cmd = chip::Platform::New<subscribe_command>(
        ptr->node_id, ptr->endpoint_id, cluster_id, attribute_id, SUBSCRIBE_ATTRIBUTE, min_interval, max_interval, true,
        attribute_data_cb, nullptr, nullptr, nullptr);

    if (!cmd) {
        ESP_LOGE(TAG, "Failed to alloc memory for subscribe-rainmaker-device command");
        return;
    }

    cmd->send_command();
}

/* subscribe device ON/OFF state. For rainmaker device, use the state in dev_list form 'get_device_list_clone()' instead.
   For local device (matter-only device), subscribe its' ON/OFF state and change on/off-line state */
void matter_ctrl_subscribe_device_state(subscribe_device_type_t sub_type)
{
    if (SUBSCRIBE_RAINMAKER_DEVICE == sub_type) {
        node_endpoint_id_list_t *dev_ptr = device_to_control.dev_list;
        while (dev_ptr) {
            if (dev_ptr->is_Rainmaker_device && dev_ptr->is_online && !dev_ptr->is_subscribed) {
                dev_ptr->is_subscribed = true;
                chip::DeviceLayer::PlatformMgr().ScheduleWork(_subscribe_rainmaker_device_state, (intptr_t)dev_ptr);
            }
            dev_ptr = dev_ptr->next;
        }
    } else if (SUBSCRIBE_LOCAL_DEVICE == sub_type) {
        /* subscribe the offline local device */
        device_list_lock my_device_lock;
        local_device_subscribe_list_t *sub_ptr = local_subscribe_list;
        while (sub_ptr) {
            if (!sub_ptr->subscribe_ptr) {
                sub_ptr->subscribe_ptr = chip::Platform::New<subscribe_command>(
                    sub_ptr->node_id, sub_ptr->endpoint_id, cluster_id, attribute_id, SUBSCRIBE_ATTRIBUTE, min_interval,
                    max_interval, true,attribute_data_cb, nullptr, subscribe_done_cb, subscribe_failed_cb);
            }
            if (!sub_ptr->subscribe_ptr) {
                ESP_LOGE(TAG, "Failed to alloc memory for subscribe-local-device command");
                return;
            }

            /* only for offline device */
            if (!sub_ptr->is_subscribed && sub_ptr->offline_time == 0u) {
                chip::DeviceLayer::PlatformMgr().ScheduleWork(_subscribe_local_device_state, (intptr_t)sub_ptr);
            }
            sub_ptr = sub_ptr->next;
        }
    }
}

static void _read_device_state(intptr_t arg)
{
    node_endpoint_id_list_t *ptr = (node_endpoint_id_list_t *)arg;
    if (!ptr) {
        ESP_LOGE(TAG, "Read device state with null ptr");
        return;
    }

    esp_matter::controller::read_command *cmd =
        chip::Platform::New<read_command>(ptr->node_id, ptr->endpoint_id, cluster_id, attribute_id,
                                          esp_matter::controller::READ_ATTRIBUTE, attribute_data_cb,nullptr, nullptr);

    if (!cmd) {
        ESP_LOGE(TAG, "Failed to alloc memory for read_command");
        return;
    }

    cmd->send_command();
}

/* For rainmaker device, read the initial ON/OFF state */
void matter_ctrl_read_device_state(void)
{
    device_list_lock my_device_lock;
    node_endpoint_id_list_t *dev_ptr = device_to_control.dev_list;
    while (dev_ptr) {
        if (dev_ptr->is_Rainmaker_device && dev_ptr->is_online && !dev_ptr->is_subscribed) {
            chip::DeviceLayer::PlatformMgr().ScheduleWork(_read_device_state, (intptr_t)dev_ptr);
            dev_ptr->is_subscribed = true;
        }
        dev_ptr = dev_ptr->next;
    }
}

static void send_command_cb(intptr_t arg)
{
    node_endpoint_id_list_t *ptr = (node_endpoint_id_list_t *)arg;
    const char *cmd_data[] = {"0x6" /* on-off-cluster*/, "0x2" /* toggle */};
    if (ptr) {
        ESP_LOGI(TAG, "send command to node %llx endpoint %d", ptr->node_id, ptr->endpoint_id);
        esp_matter::controller::send_invoke_cluster_command(ptr->node_id, ptr->endpoint_id, 2, (char **)cmd_data);
    } else
        ESP_LOGE(TAG, "send command with null ptr");
}

void matter_ctrl_send_command(intptr_t arg)
{
    chip::DeviceLayer::PlatformMgr().ScheduleWork(send_command_cb, arg);
}

/* The dev_list from 'get_device_list_clone()' does not contain attributes, maintain a local device_list */
esp_err_t matter_ctrl_get_device(void *dev_list)
{
    m_device_ptr = (matter_device_t *)dev_list;

    if (!device_list_mutex)
        device_list_mutex = xSemaphoreCreateRecursiveMutex();
    matter_device_t *dev = m_device_ptr;
    node_endpoint_id_list_t *ptr = NULL;
    node_endpoint_id_list_t *pre_ptr = NULL;
    local_device_subscribe_list_t *s_ptr = NULL;
    local_device_subscribe_list_t *pre_s_ptr = NULL;
    device_list_lock my_device_lock;

    if (!m_device_ptr) {
        ESP_LOGE(TAG, "Get NULL device list");
    }

    while (dev) {
        for (size_t i = 0; i < dev->endpoint_count; ++i) {
            node_endpoint_id_list_t *ptr = device_to_control.dev_list;
            bool search_node_flag = false;
            while (ptr) {
                if (dev->node_id == ptr->node_id && dev->endpoints[i].endpoint_id == ptr->endpoint_id) {
                    if (ptr->is_Rainmaker_device) {
                        if (!ptr->is_online && dev->reachable)
                            ++device_to_control.online_num;
                        else if (ptr->is_online && !dev->reachable)
                            --device_to_control.online_num;
                        ptr->is_online = dev->reachable;
                    }
                    ptr->is_searched = true;
                    search_node_flag = true;
                    break;
                }
                ptr = ptr->next;
            }

            if (!search_node_flag) {
                ESP_LOGE(TAG, "node-endpoint not found, creat a node!");
                size_t type = 3;
                switch (dev->endpoints[i].device_type_id) {
                case ESP_MATTER_ON_OFF_LIGHT_DEVICE_TYPE_ID:
                case ESP_MATTER_DIMMABLE_LIGHT_DEVICE_TYPE_ID:
                case ESP_MATTER_COLOR_TEMPERATURE_LIGHT_DEVICE_TYPE_ID:
                case ESP_MATTER_EXTENDED_COLOR_LIGHT_DEVICE_TYPE_ID:
                    type = CONTROL_LIGHT_DEVICE;
                    break;
                case ESP_MATTER_ON_OFF_PLUGIN_UNIT_DEVICE_TYPE_ID:
                case ESP_MATTER_DIMMABLE_PLUGIN_UNIT_DEVICE_TYPE_ID:
                    type = CONTROL_PLUG_DEVICE;
                    break;
                case ESP_MATTER_ON_OFF_SWITCH_DEVICE_TYPE_ID:
                case ESP_MATTER_DIMMER_SWITCH_DEVICE_TYPE_ID:
                case ESP_MATTER_COLOR_DIMMER_SWITCH_DEVICE_TYPE_ID:
                case ESP_MATTER_GENERIC_SWITCH_DEVICE_TYPE_ID:
                    type = CONTROL_SWITCH_DEVICE;
                    break;
                default:
                    type = CONTROL_UNKNOWN_DEVICE;
                    break;
                }

                node_endpoint_id_list_t *new_ptr = (node_endpoint_id_list_t *)malloc(sizeof(node_endpoint_id_list_t));
                if (!new_ptr) {
                    ESP_LOGE(TAG, "node-endpoint memory malloc failed!");
                    goto matter_get_device_fail;
                }
                new_ptr->next = NULL;
                new_ptr->is_subscribed = false;
                new_ptr->is_searched = true;
                new_ptr->node_id = dev->node_id;
                new_ptr->endpoint_id = dev->endpoints[i].endpoint_id;
                new_ptr->device_type = type;
                new_ptr->is_Rainmaker_device = dev->is_rainmaker_device;
                if (new_ptr->is_Rainmaker_device) {
                    new_ptr->is_online = dev->reachable;
                    if (new_ptr->is_online)
                        ++device_to_control.online_num;
                } else {
                    /* for local device, set state as offline first time */
                    new_ptr->is_online = false;

                    /* for local device, add the node to the local subscribe list */
                    local_device_subscribe_list_t *new_s_ptr =
                        (local_device_subscribe_list_t *)malloc(sizeof(local_device_subscribe_list_t));
                    if (!new_s_ptr) {
                        ESP_LOGE(TAG, "subscribe list memory malloc failed!");
                        goto matter_get_device_fail;
                    }

                    new_s_ptr->offline_time = 0;
                    new_s_ptr->is_subscribed = false;
                    new_s_ptr->is_searched = true;
                    new_s_ptr->subscribe_ptr = NULL;
                    new_s_ptr->node_id = dev->node_id;
                    new_s_ptr->endpoint_id = dev->endpoints[i].endpoint_id;

                    new_s_ptr->next = local_subscribe_list;
                    local_subscribe_list = new_s_ptr;
                }

                /* insert in tail */
                node_endpoint_id_list_t *tail = get_tail_device(device_to_control.dev_list);
                if (tail)
                    tail->next = new_ptr;
                else
                    device_to_control.dev_list = new_ptr;
                ++device_to_control.device_num;
            } else {
                if (!dev->is_rainmaker_device) {
                    local_device_subscribe_list_t *s_ptr = local_subscribe_list;
                    while (s_ptr) {
                        if (s_ptr->node_id == dev->node_id && s_ptr->endpoint_id == dev->endpoints[i].endpoint_id) {
                            s_ptr->is_searched = true;
                            break;
                        }
                        s_ptr = s_ptr->next;
                    }
                }
            }
        }
        dev = dev->next;
    }

    /* remove the not-found node from the device list */
    ptr = device_to_control.dev_list;
    pre_ptr = NULL;
    while (ptr) {
        if (!ptr->is_searched) {
            if (ptr->is_online)
                --device_to_control.online_num;
            ESP_LOGE(TAG, "node-endpoint removed!");
            if (ptr == device_to_control.dev_list) {
                device_to_control.dev_list = ptr->next;
                free(ptr);
                ptr = device_to_control.dev_list;
            } else {
                pre_ptr->next = ptr->next;
                free(ptr);
                ptr = pre_ptr->next;
            }
        } else {
            ptr->is_searched = false;
            pre_ptr = ptr;
            ptr = pre_ptr->next;
        }
    }

    /* remove the not-found node from the subscribe list */
    s_ptr = local_subscribe_list;
    pre_s_ptr = NULL;
    while (s_ptr) {
        if (!s_ptr->is_searched) {
            ESP_LOGE(TAG, "subscribe-node removed!");
            if (s_ptr == local_subscribe_list) {
                local_subscribe_list = s_ptr->next;
                free(s_ptr);
                s_ptr = local_subscribe_list;
            } else {
                pre_s_ptr->next = s_ptr->next;
                free(s_ptr);
                s_ptr = pre_s_ptr->next;
            }
        } else {
            s_ptr->is_searched = false;
            pre_s_ptr = s_ptr;
            s_ptr = pre_s_ptr->next;
        }
    }

    m_device_ptr = NULL;
    return ESP_OK;

matter_get_device_fail:
    m_device_ptr = NULL;
    ESP_LOGE(TAG, "matter get device fail!");
    return ESP_FAIL;
}
void read_dev_info(void)
{
    device_list_lock my_device_lock;
    std::vector<uint64_t> nid_list;
    node_endpoint_id_list_t *dev_ptr = device_to_control.dev_list;
    while (dev_ptr) {
        //if (dev_ptr->is_Rainmaker_device && dev_ptr->is_online && !dev_ptr->is_subscribed) 
        //{
            //chip::DeviceLayer::PlatformMgr().ScheduleWork(_read_device_state, (intptr_t)dev_ptr);
            //dev_ptr->is_subscribed = true;
        //}
        nid_list.push_back(dev_ptr->node_id);
        ESP_LOGI(TAG,"\nnodeid-> %llx\n",dev_ptr->node_id);
        dev_ptr = dev_ptr->next;
    }
    
    read_node_info(nid_list);

    nid_list.clear();
}
