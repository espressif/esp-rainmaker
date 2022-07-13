/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <stdint.h>
#include <esp_err.h>
#include <iot_button.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Create a button handle
 *
 * This is just a wrapper over iot_button_create(). This can be used to register
 * Wi-Fi/Factory reset functionality for a button.
 *
 * @param[in] gpio_num GPIO index of the pin that the button uses.
 * @param[in] active_level button hardware active level.
 *        "BUTTON_ACTIVE_LOW" means that when the button is pressed, the GPIO will read low level.
 *        For "BUTTON_ACTIVE_HIGH", it will be reverse.
 *
 * @return A button_handle_t handle to the created button object, or NULL in case of error.
 */
button_handle_t app_reset_button_create(gpio_num_t gpio_num, button_active_t active_level);

/** Register callbacks for Wi-Fi/Factory reset
 *
 * Register Wi-Fi reset or factory reset functionality on a button.
 * If you want to use different buttons for these two, call this API twice, with appropriate
 * button handles.
 *
 * @param[in] btn_handle Button handle returned by iot_button_create() or app_button_create()
 * @param[in] wifi_reset_timeout Timeout after which the Wi-Fi reset should be triggered. Set to 0,
 *              if you do not want Wi-Fi reset.
 * @param[in] factory_reset_timeout Timeout after which the factory reset should be triggered. Set to 0,
 *              if you do not want factory reset.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t app_reset_button_register(button_handle_t btn_handle, uint8_t wifi_reset_timeout, uint8_t factory_reset_timeout);

#ifdef __cplusplus
}
#endif
