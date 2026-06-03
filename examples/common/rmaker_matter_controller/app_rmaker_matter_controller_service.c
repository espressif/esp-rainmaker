/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_services.h>

#include "app_rmaker_matter_controller_service.h"

static esp_rmaker_param_t *matter_controller_rmaker_group_id_param_create(const char *param_name)
{
    esp_rmaker_param_t *param =
        esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_RMAKER_GROUP_ID, esp_rmaker_str(""),
                                PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    return param;
}

static esp_rmaker_param_t *matter_controller_matter_ctl_cmd_param_create(const char *param_name)
{
    esp_rmaker_param_t *param =
        esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_MATTER_CTL_CMD, esp_rmaker_int(-1), PROP_FLAG_WRITE);
    return param;
}

static esp_rmaker_param_t *matter_controller_matter_ctl_status_param_create(const char *param_name)
{
    esp_rmaker_param_t *param = esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_MATTER_CTL_STATUS,
                                                        esp_rmaker_int(0), PROP_FLAG_READ | PROP_FLAG_PERSIST);
    return param;
}

esp_rmaker_device_t *matter_controller_setup_service_create(const char *serv_name,
                                                            esp_rmaker_device_bulk_write_cb_t bulk_write_cb,
                                                            esp_rmaker_device_bulk_read_cb_t bulk_read_cb,
                                                            void *priv_data)
{
    esp_rmaker_device_t *service =
        esp_rmaker_service_create(serv_name, ESP_RMAKER_SERVICE_MATTER_CONTROLLER_SETUP, priv_data);
    if (service) {
        esp_rmaker_device_add_bulk_cb(service, bulk_write_cb, bulk_read_cb);
        // Use a persistent parameter for the rainmaker group id related to the Matter Fabric
        esp_rmaker_device_add_param(
            service, matter_controller_rmaker_group_id_param_create(ESP_RMAKER_DEF_RMAKER_GROUP_ID_NAME));
        esp_rmaker_device_add_param(service,
                                    matter_controller_matter_ctl_cmd_param_create(ESP_RMAKER_DEF_MATTER_CTL_CMD_NAME));
        esp_rmaker_device_add_param(
            service, matter_controller_matter_ctl_status_param_create(ESP_RMAKER_DEF_MATTER_CTL_STATUS_NAME));
    }
    return service;
}
