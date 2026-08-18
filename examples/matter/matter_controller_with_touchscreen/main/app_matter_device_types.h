#pragma once

#include <app_rmaker_matter_device_list.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MATTER_DEVICE_TYPE_LIGHT = 0,
    MATTER_DEVICE_TYPE_PLUG,
    MATTER_DEVICE_TYPE_SWITCH,
    MATTER_DEVICE_TYPE_UNKNOWN,
} matter_device_type_t;

matter_device_type_t matter_device_type_from_endpoint(const endpoint_entry_t *endpoint);
const char *matter_device_type_label(matter_device_type_t type);
bool matter_device_type_is_onoff(matter_device_type_t type);

#ifdef __cplusplus
}
#endif
