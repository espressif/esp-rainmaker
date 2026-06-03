/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <esp_rmaker_core.h>

#ifdef __cplusplus
extern "C" {
#endif

// Matter Controller service
#define ESP_RMAKER_MATTER_CONTROLLER_SETUP_SERVICE_NAME "MatterCTLSetup"
#define ESP_RMAKER_SERVICE_MATTER_CONTROLLER_SETUP "esp.service.matter-controller-setup"

#define ESP_RMAKER_DEF_RMAKER_GROUP_ID_NAME "RMakerGroupID"
#define ESP_RMAKER_PARAM_RMAKER_GROUP_ID "esp.param.rmaker-group-id"
#define ESP_RMAKER_DEF_MATTER_CTL_CMD_NAME "MTCtlCMD"
#define ESP_RMAKER_PARAM_MATTER_CTL_CMD "esp.param.matter-ctl-cmd"
#define ESP_RMAKER_DEF_MATTER_CTL_STATUS_NAME "MTCtlStatus"
#define ESP_RMAKER_PARAM_MATTER_CTL_STATUS "esp.param.matter-ctl-status"

#define MATTER_CTL_CMD_UPDATE_NOC 1
#define MATTER_CTL_CMD_UPDATE_DEVICE_LIST 2

/**
 * @brief Create rainmaker matter controller setup service
 *
 * @param[in] serv_name Name of the service
 * @param[in] bulk_write_cb Bulk write callback
 * @param[in] bulk_read_cb Bulk read callback
 * @param[in] priv_data Private data
 *
 * @return Service handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_device_t *matter_controller_setup_service_create(const char *serv_name,
                                                            esp_rmaker_device_bulk_write_cb_t bulk_write_cb,
                                                            esp_rmaker_device_bulk_read_cb_t bulk_read_cb,
                                                            void *priv_data);

#ifdef __cplusplus
}
#endif
