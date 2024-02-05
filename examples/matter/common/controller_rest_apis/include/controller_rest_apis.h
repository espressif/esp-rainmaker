// Copyright 2023 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#include <esp_err.h>
#include <matter_device.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
    CSR_TYPE_USER,
    CSR_TYPE_CONTROLLER,
} csr_type_t;

/**
 * Fetch access_token with refresh token
 *
 * @param[in] endpoint_url The base endpoint URL
 * @param[in] refresh_token The refresh token used to extend an existing session
 * @param[out] access_token The access token to be passed in the "Authorization" HTTP header as the authentication token
 * @param[in] access_token_buf_len The access_token buffer size
 *
 * @return ESP_OK on sussess
 * @return error in case of failure
 */
esp_err_t fetch_access_token(const char *endpoint_url, const char *refresh_token, char *access_token,
                             size_t access_token_buf_len);

/**
 * Fetch RainMaker Group ID corresponding to the Matter Fabric ID
 *
 * @param[in] endpoint_url The base endpoint URL
 * @param[in] access_token The access token to be passed in the "Authorization" HTTP header as the authentication token
 * @param[in] fabric_id The Matter Fabric ID
 * @param[out] group_id The RainMaker Group ID corresponding to the Matter Fabric ID
 * @param[in] group_id_buf_len The group_id buffer size
 *
 * @return ESP_OK on sussess
 * @return error in case of failure
 */
esp_err_t fetch_rainmaker_group_id(const char *endpoint_url, const char *access_token, const uint64_t fabric_id,
                                   char *group_id, size_t group_id_buf_len);

/**
 * Fetch Matter Fabric ID corresponding to the RainMaker Group ID
 *
 * @param[in] endpoint_url The base endpoint URL
 * @param[in] access_token The access token to be passed in the "Authorization" HTTP header as the authentication token
 * @param[in] group_id The RainMaker ID
 * @param[out] fabric_id The Matter Fabric ID corresponding to the RainMaker Group ID
 *
 * @return ESP_OK on sussess
 * @return error in case of failure
 */
esp_err_t fetch_matter_fabric_id(const char *endpoint_url, const char *access_token, const char *group_id,
                                 uint64_t *fabric_id);

/**
 * Fetch the Root CA certificate
 *
 * @param[in] endpoint_url The base endpoint URL
 * @param[in] access_token The access token to be passed in the "Authorization" HTTP header as the authentication token
 * @param[in] rainmaker_group_id The Rainmaker Group ID for the Matter Fabric of the new NOC
 * @param[out] rcac_der The fetched RCAC file in DER format
 * @param[in,out] rcac_der_len The length of rcac_der buffer as input and the length of issued RCAC file in DER format as output
 *
 * @return ESP_OK on sussess
 * @return error in case of failure
 */
esp_err_t fetch_fabric_rcac_der(const char *endpoint_url, const char *access_token, const char *rainmaker_group_id,
                                unsigned char *rcac_der, size_t *rcac_der_len);

/**
 * Fetch the IPK of Matter Fabric
 *
 * @param[in] endpoint_url The base endpoint URL
 * @param[in] access_token The access token to be passed in the "Authorization" HTTP header as the authentication token
 * @param[in] rainmaker_group_id The Rainmaker Group ID for the Matter Fabric of the new NOC
 * @param[out] ipk_buf The IPK of the Matter Fabric
 * @param[in] ipk_buf_size The size of ipk_buf
 *
 * @return ESP_OK on sussess
 * @return error in case of failure
 */
esp_err_t fetch_fabric_ipk(const char *endpoint_url, const char *access_token, const char *rainmaker_group_id,
                           uint8_t *ipk_buf, size_t ipk_buf_size);

/**
 * Issue Matter NOC with CSR
 *
 * @param[in] endpoint_url The base endpoint URL
 * @param[in] access_token The access token to be passed in the "Authorization" HTTP header as the authentication token
 * @param[in] csr_type The CSR type of this NOC request
 * @param[in] csr_der The CSR bytes in DER format to issue the user NOC
 * @param[in] csr_der_len The length of the CSR bytes in DER format
 * @param[in] rainmaker_group_id The Rainmaker Group ID for the Matter Fabric of the new NOC
 * @param[in,out] matter_node_id The Matter Node ID in the subject DN of the new user NOC
 * @param[out] noc_der The issued NOC file in DER format
 * @param[in,out] noc_der_len The length of noc_der buffer as input and the length of issued NOC file in DER format as output
 *
 * @return ESP_OK on sussess
 * @return error in case of failure
 */
esp_err_t issue_noc_with_csr(const char *endpoint_url, const char *access_token, csr_type_t csr_type,
                             const uint8_t *csr_der, const size_t csr_der_len, const char *rainmaker_group_id,
                             uint64_t *matter_node_id, unsigned char *noc_der, size_t *noc_der_len);

/**
 * Create a Matter Controller
 *
 * @param[in] endpoint_url The base endpoint URL
 * @param[in] access_token The access token to be passed in the "Authorization" HTTP header as the authentication token
 * @param[in] rainmaker_node_id The RainMaker Node ID of the Matter Controller
 * @param[in] rainmaker_group_id The Rainmaker Group ID for the Matter Fabric of the Controller
 * @param[out] matter_node_id The generate Matter Node ID for the Controller
 *
 * @return ESP_OK on sussess
 * @return error in case of failure
 */
esp_err_t create_matter_controller(const char *endpoint_url, const char *access_token, const char *rainmaker_node_id,
                                   const char *rainmaker_group_id, uint64_t *matter_node_id);

/**
 * Fetch Matter device list in Matter Fabric(RainMaker Group)
 *
 * Note:this function will allocate memory for the newly fetched device list
 *
 * @param[in] endpoint_url The base endpoint URL
 * @param[in] access_token The access token to be passed in the "Authorization" HTTP header as the authentication token
 * @param[in] rainmaker_group_id The Rainmaker Group ID
 * @param[out] matter_dev_list The Matter device list to be fetched
 *
 * @return ESP_OK on sussess
 * @return error in case of failure
 */
esp_err_t fetch_matter_device_list(const char *endpoint_url, const char *access_token, const char *rainmaker_group_id,
                                   matter_device_t **matter_dev_list);

#ifdef __cplusplus
} // extern "C"
#endif
