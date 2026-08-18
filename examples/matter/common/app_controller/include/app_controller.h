/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <app_rmaker_matter_controller.h>

#include <esp_err.h>
#include <esp_rmaker_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return an application-owned copy of the last successfully fetched Matter device list.
 *
 * The caller owns the returned list and must destroy it with app_rmaker_device_list_copy_destroy().
 *
 * @return Device list copy on success.
 * @return NULL if no successful update has been received or memory allocation fails.
 */
matter_device_t *app_controller_device_list_copy(void);

/**
 * @brief Add the RainMaker parameters used by the shared Matter controller example.
 *
 * @param[in] device RainMaker device to configure.
 *
 * @return ESP_OK on success.
 * @return Error code on failure.
 */
esp_err_t app_controller_set_device_params(esp_rmaker_device_t *device);

/**
 * @brief Initialize the shared Matter controller example support.
 *
 * Registers console commands, operational-credentials issuer, and the RainMaker Matter controller service. The optional
 * callback receives a temporary read-only device list after each update; copy it if it must outlive the callback.
 *
 * @param[in] callback Optional device-list update callback.
 *
 * @return ESP_OK on success or if already initialized.
 * @return Error code on failure.
 */
esp_err_t app_controller_init(matter_controller_device_list_update_callback_t callback);

#ifdef __cplusplus
}
#endif
