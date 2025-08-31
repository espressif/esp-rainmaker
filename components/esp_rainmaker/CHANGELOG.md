# Changelog

## 1.7.0

### Other changes

- Removed legacy code for esp idf versions < 5.1 as they have reached their end of life.

## 1.6.7

### Minor Change

- Allow challenge-response based user-node mapping even for Assisted Claiming. However, the same will
still not work with self claiming because the certificate gets registered with the RainMaker backend
after node connects to Wi-Fi, whereas this user-node mapping happens before that.

## 1.6.6

### Bug Fix

- User-Node mapping could fail if ESP Insights is enabled.
  -ESP Insights uses the ESP RainMaker Work Queue to send data. The queue itself is processed only after
   MQTT connection. So, any Insights message (Eg. periodic metric reporting) triggered before that would be
   queued, eventually causing it to get full. This can cause the user-node mapping to fail,
   as it also uses the same queue.
  - Fixed by adding a retry mechanism to the user-node mapping.

## 1.6.5

### New Feature

- Add support for challenge response based user node mapping during provisioning.
  This is more secure and reliable because
    - The mapping happens even before Wi-Fi credentials are sent, which ensures that the
      node is added to the user's account before it is connected to the network.
    - The node does not necessarily need to be connected to the network to be mapped.

Note that the node needs to be claimed before this can be used, as it requires the MQTT credentials
to be available. This is not a concern for private deployments since the node credentials are pre-flashed.
This feature requires the support to be available in phone apps too.
This was added in ESP RainMaker iOS app v3.4.0 and Android app v3.7.0

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


## 1.6.3

## Bugfix

- Duplicate otafetch after rebooting into new firmware after an OTA was causing a crash.
This was seen when both, `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` and `CONFIG_ESP_RMAKER_OTA_AUTOFETCH`
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
