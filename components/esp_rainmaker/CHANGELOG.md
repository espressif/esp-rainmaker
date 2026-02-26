# Changelog

## 1.10.4

### Bug Fixes

- Fixed MQTT LWT configuration after assisted claiming: When connectivity service is enabled with an unclaimed node,
  the LWT topic was initialized with the MAC-based node ID. After assisted claiming, the node ID changed but the LWT topic was not updated,
  causing MQTT connection issues. LWT is now reconfigured with the new node ID when assisted claiming succeeds.
- Fixed misleading Connectivity `Connected` param reporting: The param default was set from the MQTT connection status
  at enable time (typically false, since connectivity is enabled before `esp_rmaker_start()`).
  Params are only reported after MQTT connects, so the cloud received false initially then true after a delay.
  The default is now true to reflect the actual state when the param gets reported.

## 1.10.3

### Bug Fixes

- Fixed memory management issues (leaks, missing NULL checks, inconsistent ownership) in client data retrieval APIs.

## 1.10.2

### New Features

- Added Connectivity service so that clients can get connected/disconnected status of the node.
- Connected status is reported on receiving MQTT connected event.
- Disconnected status is reported via MQTT LWT.
- Delay for reporting connected status is configurable via `CONFIG_ESP_RMAKER_CONNECTIVITY_REPORT_DELAY`.
  This is required to handle race condition when a device reboots quickly, the broker may trigger the
  old connection's LWT (Connected=false) after the new connection reports Connected=true, resulting in incorrect status.

## 1.10.1

### Changes

- Move User Authentication part of RainMaker controller service to an independent service. 
  Components can register to `RMAKER_AUTH_SERVICE_EVENT` event to get the user token and base url updates.
- Added `esp.param.user-token-status` in the RainMaker User Auth Service to indicate user-token state:
  0 = not set; 1 = set but not verified; 2 = set and verified; 3 = set but expired/invalid.
- Because group_id is not marked `PROP_FLAG_PERSIST`, it must be stored manually in the RainMaker Controller.

## 1.10.0

### New Features

- Added BLE local control during provisioning phase.
    - Enable via `CONFIG_ESP_RMAKER_ENABLE_PROV_LOCAL_CTRL` in menuconfig.
    - Requires challenge-response to be enabled (`CONFIG_ESP_RMAKER_ENABLE_CHALLENGE_RESPONSE`).
    - Adds custom provisioning endpoints for direct parameter access over BLE:
        - `get_params`: Retrieve current device parameters.
        - `set_params`: Update device parameters.
        - `get_config`: Retrieve node configuration with fragmented transfer support for BLE MTU limits.
    - `get_params` and `get_config` return raw JSON by default. If a timestamp is provided,
      the response includes the data, timestamp, and signature for verification.
    - These handlers stay active only while in provisioning mode.
    - Recommend setting `CONFIG_APP_NETWORK_PROV_TIMEOUT_PERIOD=0` to keep device in provisioning mode
      indefinitely and hence, reachable for local control over BLE.
    - **Note**: This uses custom provisioning endpoints, not `esp_local_ctrl`, as the latter
      is designed for HTTP-based local control.

## 1.9.2

### New Features

- Add support of on-network challenge-response service for Thread devices.

## 1.9.1

### New Features

- Add support for ECDSA key type for claiming. You can choose between RSA (legacy) and ECDSA via Kconfig options
  `CONFIG_ESP_RMAKER_CLAIM_KEY_RSA`/`CONFIG_ESP_RMAKER_CLAIM_KEY_ECDSA`.
  `idf.py menuconfig -> ESP RainMaker Config -> Claiming Key Type -> RSA (2048-bit)/ECDSA (P-256)`

## 1.9.0

### New Features

- Implemented option to report 'failed' status on OTA rollback due to MQTT timeout.
    - Enable via `CONFIG_ESP_RMAKER_OTA_ROLLBACK_REPORT_FAILED` in menuconfig.
    - When enabled, if a rollback happens because MQTT did not connect within the configured timeout,
      the rolled-back firmware will report 'failed' status instead of 'rejected'.
    - Ensures backward compatibility: older firmware versions (without this feature) will not
      report any status, preventing incorrect 'rejected' reports.
    - Only applicable for `OTA_USING_TOPICS` type.
    - Implementation details:
        - New firmware stores failure reason and job ID in separate NVS keys before rollback.
        - Rolled-back firmware reads these keys and reports 'failed' status with appropriate job ID.
        - Main job ID key is erased before rollback to prevent old firmware from reporting 'rejected'.
    - **WARNING**: Use this option with caution. If the new firmware has issues that cause persistent
      MQTT connection failures, enabling this feature may cause the device to toggle between two
      firmware versions indefinitely (new firmware boots → MQTT fails → rollback → OTA retry →
      new firmware boots again).

## 1.8.9

### New Features

- Added on-network challenge-response service for user-node mapping.
    - Enable via `CONFIG_ESP_RMAKER_ON_NETWORK_CHAL_RESP_ENABLE` in menuconfig.
    - Allows mapping already-provisioned devices to user accounts without re-provisioning.
    - Starts an HTTP server (default port 80) with protocomm security (Sec0/Sec1/Sec2).
    - Devices announce themselves via mDNS service type `_esp_rmaker_chal_resp._tcp`.
    - Challenge-response endpoint: `ch_resp`.
    - TXT records include: `node_id`, `port`, `sec_version`, `pop_required`.
    - Optional instance name support via `config.mdns_instance_name` (NULL uses node_id).
    - APIs: `esp_rmaker_on_network_chal_resp_start()`, `esp_rmaker_on_network_chal_resp_stop()`,
      `esp_rmaker_on_network_chal_resp_is_running()`.
    - **Note**: Mutually exclusive with Local Control (`CONFIG_ESP_RMAKER_LOCAL_CTRL_FEATURE_ENABLE`)
      as both use protocomm_httpd which only supports one instance at a time.

- Added challenge-response endpoint in Local Control service.
    - Enable via `CONFIG_ESP_RMAKER_LOCAL_CTRL_CHAL_RESP_ENABLE` in menuconfig.
    - Registers `ch_resp` endpoint in the Local Control HTTP server.
    - Announces mDNS service `_esp_rmaker_chal_resp._tcp` (same as standalone service for consistency).
    - TXT records include: `node_id`, `port`, `sec_version`, `pop_required`.
    - Optional instance name parameter (NULL uses node_id as default).
    - APIs: `esp_rmaker_local_ctrl_enable_chal_resp(instance_name)`,
      `esp_rmaker_local_ctrl_disable_chal_resp()`.
    - Both standalone and local control ch_resp use the same mDNS service type, simplifying discovery.

- Added `esp_rmaker_local_ctrl_set_pop()` API to set custom PoP for Local Control.
    - Allows using the same PoP as provisioning for Local Control service.
    - Must be called before `esp_rmaker_local_ctrl_enable()`.

- Added console commands to enable/disable these handlers based on `CONFIG_ESP_RMAKER_CONSOLE_CHAL_RESP_CMDS_ENABLE`.
    - Console command: `chal-resp-enable [instance_name]` (instance name is optional).
    - The appropriate APIs are called based on whether `CONFIG_ESP_RMAKER_ON_NETWORK_CHAL_RESP_ENABLE` or
      `CONFIG_ESP_RMAKER_LOCAL_CTRL_CHAL_RESP_ENABLE` is enabled.

## 1.8.8

### Bug Fixes
- Fixed an issue where HTTP OTA resumption reported incorrect progress, even though the OTA itself could still succeed.
- Fixed when use ECDSA to sign the data, the device will crash.

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
