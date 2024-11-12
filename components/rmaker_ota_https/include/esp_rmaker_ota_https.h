#pragma once

#include <esp_err.h>
#include <esp_rmaker_ota.h>

/** Enables and initializes RainMaker OTA using HTTP transport. 
 * 
 * @return ESP_OK if success, failure otherwise
*/
esp_err_t esp_rmaker_ota_https_enable(esp_rmaker_ota_config_t *ota_config);

/** Checks if there is any ota available and applies the OTA if found
 * 
 * @return ESP_OK if success, failure otherwise
 */
esp_err_t esp_rmaker_ota_https_fetch(void);

/** Marks the current OTA partition as valid
 * 
 * @return ESP_OK if success, failure otherwise
 */
esp_err_t esp_rmaker_ota_https_mark_valid(void);

/** Marks the current OTA partition as invalid as reboots
 * 
 * @return ESP_OK if success, failure otherwise
 */
esp_err_t esp_rmaker_ota_https_mark_invalid(void);