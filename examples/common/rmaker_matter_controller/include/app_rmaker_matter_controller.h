/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <app_rmaker_user_api.h>
#include <esp_err.h>
#include <esp_rmaker_core.h>
#include <stdint.h>

#include "app_rmaker_matter_controller_api.h"
#include "app_rmaker_matter_device_list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*matter_controller_setup_callback_t)(uint8_t *ipk, size_t ipk_len, uint64_t fabric_id);
typedef esp_err_t (*matter_controller_update_noc_callback_t)(uint64_t fabric_id);
typedef void (*device_list_update_callback_t)(esp_err_t err);

typedef struct {
    /* This callback is used to set up matter controller. Will be called in app_rmaker_matter_controller_handle_update()
       if not setup yet and authorized successfully.

    1. For controller instance (client-only controller),
       The matter controller client setup related operations MUST be done in this callback. If the controller has not
       been successfully set up before, an IPK with ipk_len will be passed to this callback. Otherwise, NULL ipk and
       ipk_len will be passed.

    2. For server instance matter controller,
       If the controller has not been successfully set up before, updating NOC (If the provided fabric_id matches an
       existing fabric on the node) or adding a new fabric with IPK MUST be done in this callback. If has been
       successfully set up before, updating NOC or adding a new fabric is not required, but user MUST set the used
       fabric index by calling esp_matter::controller::set_fabric_index(). */
    matter_controller_setup_callback_t setup_callback;

    /* When update NOC command is received, this callback will be called. The application should update NOC in this
       callback. Can be NULL if not required. */
    matter_controller_update_noc_callback_t update_noc_callback;

    /* When device list is updated through app_rmaker_update_matter_device_list(), this callback will be called to
       notify the application. Can be NULL if not required.*/
    device_list_update_callback_t device_list_update_callback;
} matter_controller_config_t;

/**
 * @brief Enable the rainmaker matter controller
 *
 * This function will add "esp.service.matter-controller-setup" service to the rainmaker node.
 * User MUST also use esp_rmaker_auth_service_enable() to enable the user authentication service.
 * Should call this function before esp_rmaker_auth_service_enable().
 *
 * @param[in] config The configuration of the rainmaker matter controller
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_matter_controller_enable(matter_controller_config_t *config);

/**
 * @brief Update the rainmaker matter controller handle and report the matter controller status to Rainmaker cloud.
 *
 * Should call this function after GOT_IP event.
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_matter_controller_handle_update();

// TODO: Remove these storage APIs once the esp_matter component includes the commit
// for setting up the controller with the stored fabric in the esp_matter component.
// See: https://github.com/espressif/esp-matter/commit/c87613e7c6d81f2217ece49ce3b2d4904a00a653
/**
 * @brief Get the stored RCAC from the NVS.
 *
 * If failed to get RCAC from the NVS, call app_rmaker_matter_controller_fetch_rcac() to fetch it from Rainmaker cloud.
 *
 * @param[out] rcac_der The pointer to the RCAC in DER format
 * @param[in,out] rcac_der_len The length of the RCAC buffer as input and the length of the got RCAC in DER format as
 * output
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_matter_controller_get_stored_rcac(uint8_t *rcac_der, size_t *rcac_der_len);

/**
 * @brief Fetch the RCAC from Rainmaker cloud
 *
 * If RCAC is fetched successfully, it will be stored in the NVS for client-only controller. For server instance
 * controller, it is not necessary to store RCAC in NVS, because it will be stored in the Fabric Table after the Fabric
 * is committed.
 *
 * @param[out] rcac_der The pointer to the RCAC in DER format
 * @param[in,out] rcac_der_len The length of the RCAC buffer as input and the length of the got RCAC in DER format as
 * output
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_matter_controller_fetch_rcac(uint8_t *rcac_der, size_t *rcac_der_len);

/**
 * @brief Issue NOC in DER format for the rainmaker matter controller from Rainmaker cloud.
 *
 * If NOC is issued successfully, the serialized keypair and issued NOC will be stored in the NVS for client-only
 * controller. For server instance controller, it is not necessary to store NOC and keypair in NVS, because they will be
 * stored in the Fabric Table after the Fabric is committed.
 *
 * @param[in] csr_der The pointer to the CSR in DER format
 * @param[in] csr_der_len The length of the CSR
 * @param[out] noc_der The pointer to the NOC in DER format
 * @param[in,out] noc_der_len The length of the NOC buffer as input and the length of the got NOC in DER format as
 * output
 * @param[in] node_id The matter node ID for the Matter controller, only used for server instance controller and value
 * 0 (undefined node Id) means rainmaker should create a matter node for the controller. Ignored for client-only
 * controller.
 * @param[in] serialized_keypair The pointer to the serialized keypair, only used for client-only controller. Ignored
 * for server instance controller.
 * @param[in] serialized_keypair_len The length of the serialized keypair buffer, only used for client-only controller.
 * Ignored for server instance controller.
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_matter_controller_issue_controller_noc(const uint8_t *csr_der, size_t csr_der_len,
                                                            uint8_t *noc_der, size_t *noc_der_len, uint64_t node_id,
                                                            uint8_t *serialized_keypair, size_t serialized_keypair_len);

/**
 * @brief Get the stored keypair and NOC from the NVS
 *
 * If failed to get keypair and NOC from the NVS, call app_rmaker_matter_controller_issue_controller_noc() to issue NOC
 * from Rainmaker cloud.
 *
 * @param[out] noc_der The pointer to the NOC in DER format
 * @param[in,out] noc_der_len The length of the NOC buffer as input and the length of the got NOC in DER format as
 * output
 * @param[out] serialized_keypair The pointer to the serialized keypair
 * @param[in,out] serialized_keypair_len The length of the serialized keypair buffer as input and the length of the got
 * serialized keypair as output
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_matter_controller_get_stored_keypair_and_controller_noc(uint8_t *noc_der, size_t *noc_der_len,
                                                                             uint8_t *serialized_keypair,
                                                                             size_t *serialized_keypair_len);

/**
 * @brief Update the local matter device list to the latest one from Rainmaker cloud
 *
 * This function should be called after the matter controller is setup.
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t app_rmaker_update_matter_device_list();

/**
 * @brief Get the copied matter device list, should use app_rmaker_free_matter_device_list to free the returned device
 * list after use
 *
 * @return The copied matter device list on success.
 * @return NULL in case of failure.
 */
matter_device_t *app_rmaker_get_matter_device_list();

/**
 * @brief Free the allocated memory for matter device list
 *
 * @param[in] dev_list The device list to free
 */
void app_rmaker_free_matter_device_list(matter_device_t *dev_list);

/**
 * @brief Print the informations for matter device list
 *
 * @param[in] dev_list The device list to be printed
 */
void app_rmaker_print_matter_device_list(matter_device_t *dev_list);

#ifdef __cplusplus
}
#endif
