#pragma once

#include <app_rmaker_matter_device_list.h>
#include <app_rmaker_matter_report.h>
#include <esp_err.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void matter_ctrl_primary_action(uint64_t node_id, uint16_t endpoint_id);
void matter_ctrl_on_device_list_update(esp_err_t err, const matter_device_t *dev_list);
void matter_ctl_on_matter_report(const app_rmaker_matter_report_t *report, void *priv_data);
esp_err_t matter_ctrl_ui_init(void);
const char *matter_ctrl_get_qr_payload(void);
void matter_ctrl_set_qr_payload(const char *payload);
void matter_ctrl_set_provisioned(bool provisioned);
bool matter_ctrl_is_provisioned(void);

#ifdef __cplusplus
}
#endif
