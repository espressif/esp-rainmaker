/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <app_rmaker_user_api.h>
#include <esp_err.h>
#include <stddef.h>
#include <stdint.h>

#include "app_rmaker_matter_device_list.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get Matter Fabric ID corresponding to the RainMaker Group ID
 *
 * @param[in] group_id The RainMaker Group ID
 * @param[out] fabric_id The Matter Fabric ID corresponding to the RainMaker Group ID
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_api_get_matter_fabric_id(const char *group_id, uint64_t *fabric_id);

/**
 * @brief Get the RainMaker Group ID corresponding to the Matter Fabric ID
 *
 * @param[in] fabric_id The Matter Fabric ID
 * @param[in,out] group_id The RainMaker Group ID corresponding to the Matter Fabric ID. The buffer should be large
 * enough to hold the group_id.
 * @param[in] group_id_len The length of the group_id buffer
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_api_get_group_id_by_fabric(uint64_t fabric_id, char *group_id, size_t group_id_len);

/**
 * @brief Get the Root CA certificate (RCAC) of Matter Fabric
 *
 * @param[in] group_id The Rainmaker Group ID for the Matter Fabric
 * @param[out] rcac_der The fetched RCAC file in DER format
 * @param[in,out] rcac_der_len The length of rcac_der buffer as input and the length of fetched RCAC file in DER format
 * as output
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_api_get_fabric_rcac(const char *group_id, unsigned char *rcac_der, size_t *rcac_der_len);

/**
 * @brief Get the IPK (Identity Protection Key) of Matter Fabric
 *
 * @param[in] group_id The Rainmaker Group ID for the Matter Fabric
 * @param[out] ipk_buf The IPK of the Matter Fabric
 * @param[in] ipk_buf_size The size of ipk_buf
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_api_get_fabric_ipk(const char *group_id, uint8_t *ipk_buf, size_t ipk_buf_size);

/**
 * @brief Create a Matter Controller
 *
 * @param[in] rainmaker_node_id The RainMaker Node ID of the Matter Controller
 * @param[in] group_id The Rainmaker Group ID for the Matter Fabric of the Controller
 * @param[out] matter_node_id The generated Matter Node ID for the Controller
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_api_create_matter_controller(const char *rainmaker_node_id, const char *group_id,
                                                  uint64_t *matter_node_id);

/**
 * @brief Issue Matter NOC (Node Operational Certificate) with CSR
 *
 * @param[in] csr_der The CSR bytes in DER format to issue the NOC
 * @param[in] csr_der_len The length of the CSR bytes in DER format
 * @param[in] group_id The Rainmaker Group ID for the Matter Fabric of the new NOC
 * @param[in] matter_node_id The Matter Node ID.
 * @param[out] noc_der The issued NOC file in DER format
 * @param[in,out] noc_der_len The length of noc_der buffer as input and the length of issued NOC file in DER format as
 * output
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_api_issue_noc(const uint8_t *csr_der, size_t csr_der_len, const char *group_id,
                                   uint64_t *matter_node_id, uint8_t *noc_der, size_t *noc_der_len);

/**
 * @brief Get Matter device list in Matter Fabric (RainMaker Group)
 *
 * Note: This function will allocate memory for the newly fetched device list.
 *       The caller is responsible for freeing the memory.
 *
 * @param[in] group_id The Rainmaker Group ID
 * @param[out] device_list The Matter device list to be fetched
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_api_get_matter_device_list(const char *group_id, matter_device_t **device_list);

#ifdef __cplusplus
}
#endif
