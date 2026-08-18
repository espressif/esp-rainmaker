#pragma once

#include <button_gpio.h>
#include <esp_err.h>
#include <esp_matter.h>
#include <iot_button.h>
#include <led_indicator.h>

/**
 * @brief Initialize the configured board LED.
 *
 * @return LED handle on success.
 * @return NULL on failure or when no LED is configured.
 */
led_indicator_handle_t app_end_device_led_init(void);

/**
 * @brief Set the board LED's RGB output.
 *
 * Single-channel LEDs use the RGB value as an on/off state.
 *
 * @param[in] handle LED handle returned by app_end_device_led_init().
 * @param[in] red Red intensity.
 * @param[in] green Green intensity.
 * @param[in] blue Blue intensity.
 *
 * @return ESP_OK on success.
 * @return Error code on failure.
 */
esp_err_t app_end_device_led_set_rgb(led_indicator_handle_t handle, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Initialize the configured board button.
 *
 * The application is responsible for registering button callbacks.
 *
 * @return Button handle on success.
 * @return NULL on failure.
 */
button_handle_t app_end_device_button_init(void);

/**
 * @brief Initialize the Matter end-device node.
 *
 * Creates the Matter node and installs the callbacks provided by the application.
 *
 * @param[in] app_attribute_update_cb Attribute update callback.
 * @param[in] app_identification_cb Identification callback.
 *
 * @return ESP_OK on success.
 * @return Error code on failure.
 */
esp_err_t app_end_device_init(esp_matter::attribute::callback_t app_attribute_update_cb,
                              esp_matter::identification::callback_t app_identification_cb);

/**
 * @brief Start the Matter stack for an initialized end-device example.
 *
 * @param[in] app_event_cb Matter device event callback.
 *
 * @return ESP_OK on success.
 * @return Error code on failure.
 */
esp_err_t app_end_device_start(esp_matter::event_callback_t app_event_cb);

/**
 * @brief Initialize RainMaker services for the end-device example.
 *
 * @return ESP_OK on success.
 * @return Error code on failure.
 */
esp_err_t app_end_device_rmaker_init(void);

/**
 * @brief Start RainMaker after network provisioning setup is complete.
 *
 * @return ESP_OK on success.
 * @return Error code on failure.
 */
esp_err_t app_end_device_rmaker_start(void);

/**
 * @brief Register Matter console commands used for local debugging.
 */
void app_end_device_enable_matter_console(void);
