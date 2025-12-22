/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_services.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_groups.h>
#include "esp_rmaker_internal.h"

static const char *TAG = "esp_rmaker_groups";

static esp_err_t groups_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_write_req_t write_req[],
        uint8_t count, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    for (uint8_t i = 0; i < count; i++) {
        if (strcmp(esp_rmaker_param_get_name(write_req[i].param), ESP_RMAKER_DEF_GROUP_ID_NAME) == 0) {
            esp_rmaker_param_update(write_req[i].param, write_req[i].val);
            esp_rmaker_register_for_group_params(write_req[i].val.val.s);
        }
    }
    return ESP_OK;
}

esp_err_t esp_rmaker_groups_service_enable(void)
{
    char *group_id = NULL;
    esp_rmaker_device_t *service = NULL;
    esp_err_t err = esp_rmaker_get_stored_group_id(&group_id);
    if (err == ESP_OK && group_id) {
        service = esp_rmaker_create_groups_service("Groups", groups_write_cb, group_id, NULL);
        free(group_id);
    } else {
        service = esp_rmaker_create_groups_service("Groups", groups_write_cb, "", NULL);
    }
    if (!service) {
        return ESP_ERR_NO_MEM;
    }
    err = esp_rmaker_node_add_device(esp_rmaker_get_node(), service);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add Groups service to node");
        esp_rmaker_device_delete(service);
        return err;
    }
    ESP_LOGD(TAG, "Groups Service Enabled");
    return ESP_OK;
}
