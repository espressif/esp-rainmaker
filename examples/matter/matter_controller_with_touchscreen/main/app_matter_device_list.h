/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <app_matter_device_types.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool onoff;
} matter_onoff_state_t;

typedef union {
    matter_onoff_state_t onoff;
} matter_device_state_t;

typedef struct matter_device_list_node {
    uint64_t node_id;
    bool is_online;
    uint16_t endpoint_id;
    matter_device_type_t device_type;
    char name[33];
    matter_device_state_t state;
    struct matter_device_list_node *next;
} matter_device_list_node_t;

typedef struct {
    size_t device_num;
    size_t online_num;
    matter_device_list_node_t *dev_list;
} matter_device_list_state_t;

void matter_device_list_lock(void);
void matter_device_list_unlock(void);
void matter_device_list_rebuild(const matter_device_t *dev_list);
void matter_device_list_fetch(void);
matter_device_list_node_t *matter_device_list_find_locked(uint64_t node_id, uint16_t endpoint_id);
bool matter_device_list_set_online_locked(matter_device_list_node_t *node, bool online);
bool matter_device_list_set_node_online(uint64_t node_id, bool online);

extern matter_device_list_state_t matter_device_list;

#ifdef __cplusplus
}
#endif
