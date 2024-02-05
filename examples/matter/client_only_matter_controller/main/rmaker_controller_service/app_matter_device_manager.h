/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_matter_console.h>
#include <matter_device.h>
#include <stdint.h>

#include <app_matter_controller.h>

namespace esp_matter {
namespace console {

/** Add controller device manager commands
 *
 * Add the default controller device manager commands
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t ctl_dev_mgr_register_commands();

} // namespace console
} // namespace esp_matter

typedef void (*device_list_update_callback_t)(void);

esp_err_t update_device_list(matter_controller_handle_t *controller_handle);

matter_device_t *fetch_device_list();

esp_err_t init_device_manager(device_list_update_callback_t dev_list_update_cb);
