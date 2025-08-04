# Changelog

## 1.6.4

## New feature

- Support for OTA over MQTT: The regular OTA upgrades with ESP RainMaker required an
additional https connection to fetch the OTA image, which has a significant impact on
heap usage. With MQTT OTA, the same MQTT channel used for rest of the RainMaker
communication is used to fetch the OTA image, thereby saving on RAM.
This could be slightly slower and may odd some cost. but the overall
impact would be low enough when compared against the advantages. This can be enabled by setting
`CONFIG_ESP_RMAKER_OTA_USE_MQTT` to `y` in the menuconfig.
(`idf.py menuconfig ->  ESP RainMaker Config -> ESP RainMaker OTA Config -> OTA Update Protocol Type -> MQTT`)

## Bugfix

- Fix a bug where the OTA fetch was not working when `CONFIG_ESP_RMAKER_OTA_AUTOFETCH` was enabled.


## 1.6.3

## Bugfix

- Duplicate otafetch after rebooting into new firmware after an OTA was causing a crash.
This was seen when both, CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE and CONFIG_ESP_RMAKER_OTA_AUTOFETCH
are enabled.

## 1.6.2

## Minor changes

- AVoid execution of `esp_rmaker_params_mqtt_init()` from esp-idf event handler

## 1.6.1

## Minor changes

- Make cmd response payload publish api (`esp_rmaker_cmd_response_publish()`) public

## 1.6.0

### Enhancements

- Enhance OTA fetch reliability
    - Monitor message publish acknowledgement for the otafetch message
    - Add retry logic if otafetch fails
- Add OTA retry on failure mechanism
    - Try OTA multiple times (as per `CONFIG_ESP_RMAKER_OTA_MAX_RETRIES`, set to 3 by default) if it fails
    - Schedule an OTA fetch as per `CONFIG_ESP_RMAKER_OTA_RETRY_DELAY_MINUTES` if all retries fail
