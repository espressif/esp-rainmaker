# Changelog

## 1.8.7

### Bug Fixes
- If assisted claiming was already performed before calling `esp_rmaker_start()`,
  the firmware was stalling with an incorrect print saying that the Node connected to network
  without claiming.

## 1.8.6

### New Feature

- Added Groups service to enable direct MQTT communication between nodes and clients (phone apps/controllers).
    - Setting group_id via the Groups service routes local param updates to group-specific topics
      (`node/<node_id>/params/local/<group_id>`) for lower latency.
    - Also added `esp_rmaker_publish_direct()` API to publish messages on `node/<node_id>/direct/params/local/<group_id>`
      for direct client-to-node communication, bypassing cloud processing for reduced cost and latency.
    - Clients can also publish param updates directly to `node/<node_id>/params/remote/<group_id>` instead of
      using the set params API.

## 1.8.5

### Changes

- Assisted claiming is now the default for all supported platforms (requires Bluetooth enabled and not ESP32S2).
  Previously, it was only the default for ESP32. To revert to self claiming, set `CONFIG_ESP_RMAKER_SELF_CLAIM=y` in menuconfig.
- Challenge response based user-node mapping is now enabled by default. This replaces the traditional user-node mapping flow.
  To disable it, set `CONFIG_ESP_RMAKER_ENABLE_CHALLENGE_RESPONSE=n` in menuconfig.
  Note that challenge response requires the node to be claimed before it can be used.

## 1.8.4

### New Feature
- Added RainMaker user Auth Service
- Added helper layer for RainMaker Controller

## 1.8.1

### New Feature

- Added optional `readme` field to node info: Allows example projects to include
  a README URL in the node configuration. The readme field will be included in
  node config only if its value is not NULL and not an empty string. This can
  be set using the `esp_rmaker_node_add_readme()` API.

## 1.8.0

### Changes
- Decouple esp_rcp_update from esp_rainmaker and it is not a core functionality of RainMaker

## 1.7.9

### New Feature
- Added set-param, get-param and update-param console commands to assist in some testing

## 1.7.8

## Changes
- Make version dependency for network_provisioning more flexible

## 1.7.7

### New Feature
- Added AWS credential provider APIs, required for RainMaker Camera applications.

## 1.7.6

## Changes
-  Disabled esp_bt code from ota code for supporting esp32-p4, which uses network adapters for Bluetooth.

## 1.7.5

### Bug Fixes
- Some guards for `CONFIG_ESP_RMAKER_CMD_RESP_ENABLE` and `CONFIG_ESP_RMAKER_PARAM_CMD_RESP_ENABLE` were missing.

### Other changes
- Changed default MQTT Host URL to mqtt.rainmaker.espressif.com to match the domain conigured on public RainMaker.


## 1.7.4

### New Feature

- Add `network-id` attribute to Thread rainmaker nodes.

This attribute represents the partition ID of the Thread network, which is used to determine whether Thread nodes belong to the same network.

## 1.7.3

### New Feature

- Add parameter updates via command-response (command id 1): Enables triggering parameter
  updates through the command-response framework with proper JSON response containing
  updated parameter values. Controlled by `CONFIG_ESP_RMAKER_PARAM_CMD_RESP_ENABLE`
  (enabled by default when command-response is enabled). This will also update the values
  in the params DB returned via the regular get params APIs like GET /user/nodes/params.
  - Sample command payload: `{"params":{"Light":{"Power":false}}}`
  - Sample response payload: `{"status":"success","params":{"Light":{"Power":false}}}`

## 1.7.2

### New Feature

- Add RainMaker OTA resumption feature. The OTA process will resume downloading from the last position instead of restarting the download.

OTA will check whether the MD5 values of the two consecutive files are the same. If the same, the OTA will continue; if they are different, the OTA will restart.
Therefore, cloud support for delivering the file MD5 value is required (supported in backend version 3.0.0 and above).
In addition, this feature requires IDF version 5.5 and above.

## 1.7.1

### New Feature

- **Daylight (Sunrise/Sunset) Schedules**: Added support for scheduling device actions based on astronomical calculations for sunrise and sunset times.
  - Supports scheduling at specific offsets from sunrise/sunset (e.g., 30 minutes before sunset)
  - Works with day-of-week repeat patterns (e.g., "every weekday at sunset")
  - Requires geographical location (latitude/longitude) for calculations. The location name is optional.
  - Configurable via `CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT` and `CONFIG_ESP_RMAKER_SCHEDULE_ENABLE_DAYLIGHT`
  - Adds about 15KB of flash space when enabled.
  - **Schedule service now includes `daylight_support` attribute** with value set to "yes" to indicate if daylight schedules are supported, enabling client applications to discover this capability.

  **Example JSON for sunrise schedule:**
  ```json
  {
    "Schedule Name": {
      "triggers": [{
        "sr": 30,
        "lat": 37.7749,
        "lon": -122.4194,
        "loc": "San Francisco",
        "d": 127,
        "ts": 1641123456
      }],
      "action": {
        "Light": {
          "Power": true,
          "Brightness": 80
        }
      }
    }
  }
  ```

  **Example JSON for sunset schedule:**
  ```json
  {
    "Evening Schedule": {
      "triggers": [{
        "ss": -15,
        "lat": 51.5074,
        "lon": -0.1278,
        "loc": "London",
        "d": 31,
        "ts": 1641123456
      }],
      "action": {
        "Light": {
          "Power": true,
          "Brightness": 50
        }
      }
    }
  }
  ```

  **JSON Field Descriptions:**
  - `sr`: Sunrise offset in minutes (positive = after sunrise, negative = before, zero = exact)
  - `ss`: Sunset offset in minutes (positive = after sunset, negative = before, zero = exact)
  - `lat`: Latitude in decimal degrees (-90 to +90, positive North)
  - `lon`: Longitude in decimal degrees (-180 to +180, positive East)
  - `loc`: Optional location name for reference
  - `d`: Day pattern bitmask (1=Sunday, 2=Monday, 4=Tuesday, ..., 127=everyday)
  - `ts`: Next trigger timestamp (automatically calculated). Can be used by the phone apps to display the next trigger time.

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
