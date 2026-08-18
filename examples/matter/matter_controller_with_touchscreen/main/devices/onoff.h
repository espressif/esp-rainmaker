#pragma once

#include <app_matter_device_list.h>
#include <app_rmaker_matter_report.h>
#include <esp_err.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void matter_onoff_on_attr_report(const app_rmaker_matter_report_t *report);
esp_err_t matter_onoff_primary_action(uint64_t node_id, uint16_t endpoint_id);

#ifdef __cplusplus
}
#endif
