#pragma once

#include <app_matter_device_list.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t node_id;
    uint16_t endpoint_id;
    matter_device_type_t device_type;
    char name[33];
    bool is_online;
    matter_device_state_t state;
} matter_device_vm_item_t;

typedef struct {
    size_t device_count;
    size_t online_count;
    bool fetchable;
} matter_device_vm_status_t;

bool matter_vm_get_status(matter_device_vm_status_t *out);
bool matter_vm_copy_devices(matter_device_vm_item_t *out, size_t max_count, size_t *out_count);
bool matter_vm_get_device(uint64_t node_id, uint16_t endpoint_id, matter_device_vm_item_t *out);
void matter_vm_notify_refresh(void);
void matter_vm_notify_loading(void);

#ifdef __cplusplus
}
#endif
