/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SUBSCRIBE_ALL_DEVICE = 0,
    SUBSCRIBE_LOCAL_DEVICE,
    SUBSCRIBE_RAINMAKER_DEVICE,
} subscribe_device_type_t;

typedef enum{
    CONTROL_LIGHT_DEVICE = 0,
    CONTROL_PLUG_DEVICE,
    CONTROL_SWITCH_DEVICE,
    CONTROL_UNKNOWN_DEVICE,
} control_device_type;

typedef struct node_endpoint_id_list {
    uint64_t node_id;
    bool is_searched;
    bool OnOff;
    volatile bool is_online;
    bool is_Rainmaker_device;
    uint16_t endpoint_id;
    size_t device_type;
    lv_obj_t *lv_obj;
    struct node_endpoint_id_list *next;
} node_endpoint_id_list_t;

typedef struct device_to_control_list {
    size_t device_num;
    size_t online_num;
    node_endpoint_id_list_t *dev_list;
} device_to_control_t;

void matter_device_list_lock();
void matter_device_list_unlock();
void matter_ctrl_lv_obj_clear();
void matter_ctrl_change_state(intptr_t arg);
void matter_ctrl_read_device_state();
esp_err_t matter_ctrl_get_device(void *dev_list);
void matter_ctrl_subscribe_device_state(subscribe_device_type_t sub_type);

void read_dev_info(void);

#ifdef __cplusplus
}
#endif
