/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_err.h>
#include <esp_matter.h>


/** Default attribute values used by Rainmaker during initialization */
#define SWITCH_DEVICE_NAME "Matter Switch"
#define DEFAULT_POWER true

typedef void *app_driver_handle_t;

/** Initialize the light driver
 *
 * This initializes the light driver associated with the selected board.
 *
 * @return Handle on success.
 * @return NULL in case of failure.
 */
app_driver_handle_t app_driver_light_init();

/** Initialize the button driver
 *
 * This initializes the button driver associated with the selected board.
 *
 * @param[in] user_data Custom user data that will be used in button toggle callback.
 *
 * @return Handle on success.
 * @return NULL in case of failure.
 */
app_driver_handle_t app_driver_button_init(void *user_data);

/** Set LED Power
 *
 * @param[in] handle Pointer to switch driver handle.
 * @param[in] power LED power state.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t app_driver_switch_set_power(app_driver_handle_t handle, bool power);
