# Changes

## 24-Jun-2024: OTA Reliability Improvements

 - Check details in esp_rainmaker component's [CHANGELOG](components/esp_rainmaker/CHANGELOG.md).
 - Subsequent major changes in component will be maintained under component CHANGELOG.

## 27-Sep-2024: Modified esp-rainmaker examples to use partitions_4mb_optimised.csv
 - RainMaker examples now use `partitions_4mb_optimised.csv` by default instead of `partitions.csv` and it is recommended to use the same in any new RainMaker projects.
 - The fctry partition address is different for this partition table. So, if the host claiming is used, please pass the new address using:
 ```bash
esp-rainmaker-cli claim $ESPPORT --address 0x3fa000
 ```

## 7-Aug-2024: Enabled ESP-Insights command response feature
 - With ESP-Insights updated to the newer component, the command response feature from rainmaker can be used.
 - To use the same, in addition to enabling esp-insights, please set below option from menuconfig:
 ```bash
CONFIG_ESP_INSIGHTS_CMD_RESP_ENABLED=y
 ```
 - More info on this can be found [here](https://github.com/espressif/esp-insights/blob/main/FEATURES.md#command-response).

## 18-Jul-2024: Use network_provsioning for ESP-IDF v5.1 or later to support RainMaker over Thread
- The network_provisioning component can be used for provisioning both Wi-Fi or Thread devices. It also stays backward capabitable with wifi_provisioning component.

## 27-Feb-2024: Add support for closing provisioning window after PoP mismatch
 - For ESP IDF v5.1.3 and later, provisioning will be stopped if there are 5 attempts to establish secure session with wrong PoP. This count can be set to any value between 0 and 20. 0 means that provisioning will not be stopped (which will be same as the earlier behaviour before this change).

## 21-Nov-2022 (esp_rmaker_mqtt: Add MQTT budgeting to control the number of messages sent)

- Due to some poor, non-optimised coding or bugs, it is possible that the node keeps bombarding the MQTT
broker with publish messages. To prevent this, a concept of MQTT Budgeting has been added.
- By default, a node will be given a budget of 100 (`CONFIG_ESP_RMAKER_MQTT_DEFAULT_BUDGET`), which will
  go on incrementing by 1 (`CONFIG_ESP_RMAKER_MQTT_BUDGET_REVIVE_COUNT` every 5 seconds (`CONFIG_ESP_RMAKER_MQTT_BUDGET_REVIVE_PERIOD`),
  limited to a max value of 1024 (`CONFIG_ESP_RMAKER_MQTT_MAX_BUDGET`).
- Budget will be decremented by 1 for every MQTT publish and messages will be dropped if budget is 0.
- This behaviour is enabled by default and can be disabled by disabling `CONFIG_ESP_RMAKER_MQTT_ENABLE_BUDGETING`.

## 16-Nov-2022 (mqtt_topics: Added support for AWS basic ingest topics.)

- AWS Basic Ingest Topics optimize data flow by removing the publish/subscribe message broker from the ingestion path, making it more cost effective. You can refer the official docs [here](https://docs.aws.amazon.com/iot/latest/developerguide/iot-basic-ingest.html).
- This setting is turned on by default and can be turned off by running `idf.py menuconfig` and disabling `CONFIG_ESP_RMAKER_MQTT_USE_BASIC_INGEST_TOPICS` option.

## 2-Nov-2022 (Added MQTT disconnect and user node mapping reset calls on WiFi/Factory Reset.)

- On a Wi-Fi reset triggered via esp_rmaker_wifi_reset(), the rmaker core will first disconnect from MQTT so that its offline state reflects immediately.
- On a Factory reset triggered via esp_rmaker_factory_reset(), the rmaker core will trigger a user mapping reset (if mqtt connection is active) so that the node gets removed from the user's account immediately.
- The reset delay time for the push button based reset has been changed from 0 to 2 seconds to give some time for the above mentioned operations.

Note: This config is enabled by default. You can disable it using `idf.py menuconfig`
## 28-Jun-2022 (examples: Enable CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE in all examples)

`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` has been enabled in all examples by default,
as a safety measure to prevent devices getting bricked after a faulty firmware upgrade.
The OTA firmware upgrade will be marked as successful only if the firmware can connect to
MQTT within 90 seconds of calling `esp_rmaker_ota_enable_default()` (or other esp_emaker_enable APIs).
The time out is configurable using `CONFIG_ESP_RMAKER_OTA_ROLLBACK_WAIT_PERIOD`.

Note that this is a bootloader feature and so, just enabling this feature and pushing out updated
firmware to existing devices won't be of any use. Please flash new bootloader on the devices to
make this work.

## 26-May-2022 (claiming and ota)

- claiming: Make self claiming as the default for esp32s3 and esp32c3
- ota: Make "OTA using Topics" as default and provide a simplified API for that

Self claiming is much more convenient and fast since the node directly gets the
credentials from the claiming service over HTTPS, instead of using the slower BLE based
Assisted claiming, wherein the phone app acts as a proxy between the node and the
claiming service. However, with self claiming, there was no concept of
[Admin Role](https://rainmaker.espressif.com/docs/user-roles.html#admin-users) and so, it was
not possible to access the node via the RainMaker or Insights dashboards. This was one
reason why Assisted Claiming was kept as a default for esp32c3 and esp32s3 even though
they support self claiming.

With recent changes in the Public RainMaker backend, the primary user (the user who performs the [user-node
mapping](https://rainmaker.espressif.com/docs/user-node-mapping.html)) for a self claimed
node is now made as the admin. This gives the primary user the access to the node for OTA and Insights.
So, self claiming has now been made as the default for all chips (except esp32) and the OTA Using Topics
has also been made as the default, since it is convenient and also the correct option for
production devices. A simpler API `esp_rmaker_ota_enable_default()` as also been added in esp_rmaker_core.h.

Note: Nodes that are already claimed via Assisted/Host Claiming will not have any effect, even if the
new firmware is enabled with self claiming. The self claiming will take effect only if the flash is
erased. **This will result in a change of node_id, since mac address is the node_id for self claimed nodes.**
If you want to contine using Assisted Claiming (probably because there is quite some data associated
with the node_id), please set is explicitly in your sdkconfig.

## 25-Jan-2022 (app_wifi: Minor feature additions to provisioning workflow)

Added a 30 minute timeout for Wi-Fi provisioning as a security measure. A device reboot will be
required to restart provisioning after it times out. The value can changed using the
`CONFIG_APP_WIFI_PROV_TIMEOUT_PERIOD` config option. A value of 0 will disable the timeout logic.
`APP_WIFI_EVENT_PROV_TIMEOUT` event will be triggerd to indicate that the provisioning has timed out.

## 25-Jan-2022 (examples: Enable some security features and change order of component dirs)

A couple of security features were added some time back, viz.

1. esp_rmaker_local_ctrl: Added support for sec1
2. esp_rmaker_user_mapping: Add checks for user id for better security

These are kept disabled by default at component level to maintain backward compatibility and not
change any existing projects. However, since enabling them is recommended, these are added in
the sdkconfig.defaults of all examples.

A minor change in CMakeLists.txt has also been done for all examples so that the rmaker_common
component from esp-rainmaker gets used, rather than the one from esp-insights.

## 12-Jan-2022 (esp_rmaker_local_ctrl: Added support for sec1)

This commit adds support for security1 for local control. This can be enabled by setting
`CONFIG_ESP_RMAKER_LOCAL_CTRL_SECURITY_1` when using local control feature (this is the
default security level when enabling local control). This would also require the latest
phone apps which have the support for security1.

You can check the docs [here](https://rainmaker.espressif.com/docs/local-control.html) for more details.

## 24-Aug-2021 (esp_rmaker_user_mapping: Add checks for user id for better security)

This commit adds some logic to detect a reset to factory or a user change during the
user node association workflow so that the RainMaker cloud can reset any earlier
mappings if this reset state is reported by the device during user node association.

If an existing, provisioned node is upgraded with a new firmware with this logic enabled,
it will send a request to cloud to reset the user mapping and so a re-provisioning would be
required. Moreover, a simple Wi-Fi reset will be treated as a factory reset in the context of
user node association. A side effect of this would be that the cloud can remove the secondary
users associated with that node and so, those would have to be added back again.
All subsequent Wi-Fi/Factory resets and provisioning + user node association would work fine.
For all new nodes, enabling this logic would have no issues.

Since this change in behavior is a breaking change, the feature has been kept disabled by default.
However, it is strongly recommended to enable this using CONFIG_ESP_RMAKER_USER_ID_CHECK

## 02-Jul-2021 (esp_insights: Add facility to enable esp_insights in the examples)

This commit introduces a breaking change in compilation, not due to any API change,
but introduction of new components under components/esp-insights/components.
You can either choose to include these components in your projects CMakeLists.txt
as per the standard examples as given here:

```
set(EXTRA_COMPONENT_DIRS ${RMAKER_PATH}/components ${RMAKER_PATH}/examples/common ${RMAKER_PATH}/components/esp-insights/components)
```

You will also have to pull in the new esp-insights submodule by executing this command:

```
git submodule update --init --recursive
```

Another option is to exclude the common example component (app_insights) that adds these
components to the dependencies by adding this to your project's CMakeLists.txt:

```
set(EXCLUDE_COMPONENTS app_insights)
```

Check out the [esp-insights](https://github.com/espressif/esp-insights) project to understand more about this.
You can also check the docs [here](https://rainmaker.espressif.com/docs/esp-insights.html) to get started with enabling Insights in ESP RainMaker.

## 28-May-2021 (esp_rmaker_core: Add a system service for reboot/reset)

The reboot/reset API prototypes have changed from

```
esp_err_t esp_rmaker_reboot(uint8_t seconds);
esp_err_t esp_rmaker_wifi_reset(uint8_t seconds);
esp_err_t esp_rmaker_factory_reset(uint8_t seconds);
```
To

```
esp_err_t esp_rmaker_reboot(int8_t seconds);
esp_err_t esp_rmaker_wifi_reset(int8_t reset_seconds, int8_t reboot_seconds);
esp_err_t esp_rmaker_factory_reset(int8_t reset_seconds, int8_t reboot_seconds);
```

- The behavior of `esp_rmaker_reboot()` has changed such that passing a value of 0 would trigger
an immediate reboot without starting any timer.
- The `esp_rmaker_wifi_reset()` and `esp_rmaker_factory_reset()` APIs have been modified such that
they now accept 2 time values. The `reset_seconds` specify the time after which the reset should trigger
and the `reboot_seconds` specify the time after which the reboot should trigger, after the reset
was done.
- `reboot_seconds` is similar to the earlier `seconds` argument, but it allows for 0 and negative values.
0 indicates that the reboot should happen immediately after reset and negative value indicates that the
reboot should be skipped.

Please refer the [API documentation](https://docs.espressif.com/projects/esp-rainmaker/en/latest/c-api-reference/rainmaker_common.html#utilities) for additional details.

## 1-Feb-2021 (esp_rmaker: Moved out some generic modules from esp_rainmaker component)

Some generic code has been moved out of the esp_rainmaker repo and included as submodules at
components/rmaker_common and cli/.

To get these submodules, you will now have to execute `git submodule update --init --recursive` once.

For new clones, use `git clone --recursive https://github.com/espressif/esp-rainmaker.git`

### RainMaker Events

As part of the above changes, the following events have changed

- RMAKER_EVENT_MQTT_CONNECTED -> RMAKER_MQTT_EVENT_CONNECTED
- RMAKER_EVENT_MQTT_DISCONNECTED -> RMAKER_MQTT_EVENT_DISCONNECTED
- RMAKER_EVENT_MQTT_PUBLISHED -> RMAKER_MQTT_EVENT_PUBLISHED

Moreover, the event base for the MQTT events has changed from `RMAKER_EVENT` to `RMAKER_COMMON_EVENT`. The base has similarly changed even for the following:

- RMAKER_EVENT_REBOOT
- RMAKER_EVENT_WIFI_RESET
- RMAKER_EVENT_FACTORY_RESET

## 16-Oct-2020 (json: Use upstream json_generator and json_parser as submodules)

To get these submodules, you will now have to execute `git submodule update --init --recursive` once.

For new clones, use `git clone --recursive https://github.com/espressif/esp-rainmaker.git`

## 16-Oct-2020 (app_wifi: Changes in SSID and PoP generation for Provisioning)

The PoP for Wi-Fi provisioning was being fetched from a random 8 character hex string stored in the fctry partition.
In this commit, the random 8 character hex string has been replaced by 64 byte random number, which can be used for other purposes as well.
PoP is now generated by reading the first 4 bytes of this and converting to 8 character hex string.
Even the SSID now uses the last 3 bytes of this random number as the suffix, instead of last 3 bytes of MAC address.
With this change, it will now be possible to generate the complete Provisioning QR code payload outside the device,
without having to know its MAC address.

## 29-Sep-2020 (esp_rmaker_standard_types: Start default names of all standard params with capital letter)

Default parameter names like name, power, etc. have been changed to Name, Power, etc. respectively, so that they look better in the phone app UIs.

With this change, any user configured device name (the name set from phone apps), or any other persistent parameter for which a
default name was used (Eg. power) will be affected, as the values will no more be found in the NVS storage.
Please edit your application code accordingly if you want to stick with the old names.

Eg. If you were using the standard lightbulb device API which internally creates power and name parameters
```
light_device = esp_rmaker_lightbulb_device_create("Light", NULL, DEFAULT_POWER);
```

Please change to below, if you want to stick with old parameter names "name" and "power".

```
light_device = esp_rmaker_device_create("Light", ESP_RMAKER_DEVICE_LIGHTBULB, NULL);
esp_rmaker_device_add_param(light_device, esp_rmaker_name_param_create("name", "Light"));
esp_rmaker_param_t *power_param = esp_rmaker_power_param_create("power", DEFAULT_POWER);
esp_rmaker_device_add_param(light_device, power_param);
esp_rmaker_device_assign_primary_param(light_device, power_param);
```

## 5-Aug-2020 (wifi_provisioning: Use a random pop instead of creating it from MAC address)

Till date, the last 4 bytes of the MAC address were being used to generate the 8 character Proof of Possession (PoP) PIN for Wi-Fi provisioning. This is not secure enough because MAC address is a public information and can also be sniffed easily by devices in vicinity. A minor risk in this is that somebody else in the vicinity can provision your device, but a major risk is a man in the middle attack, wherein someone in vicinity can read the data being exchanged between a phone and the device and get the Wi-Fi credentials.

To prevent this, it is best to use a randomly generated PoP which cannot be guessed. So now, a random stream of bytes is generated and flashed in the fctry partition during claiming and then used as PoP. If your device is already claimed, it is recommended to erase the flash and perform the claiming again. If you erase the flash again, the PoP will change. However, if you just do a reset to factory, it will not.

If for some reason, you want to continue using the earlier mac address based method, please pass `POP_TYPE_MAC` to the `esp_err_t app_wifi_start(app_wifi_pop_type_t pop_type)` function.

## 31-July-2020 (esp\_rainmaker\_core: Code restructure and API changes)

Recently we made some significant changes to most ESP RainMaker APIs to make them even more modular and object oriented. You can check the examples to see what has changed, but here is a guide to help you understand some major changes.

### RainMaker Initialisation

#### Old
```
typedef struct {
    char *name;
    char *type;
    char *fw_version;
    char *model;
} esp_rmaker_node_info_t;

typedef struct {
    esp_rmaker_node_info_t info;
    bool enable_time_sync;
} esp_rmaker_config_t;

esp_err_t esp_rmaker_init(esp_rmaker_config_t *config);
```

#### New
```
typedef struct {
    bool enable_time_sync;
} esp_rmaker_config_t;

esp_rmaker_node_t *esp_rmaker_node_init(const esp_rmaker_config_t *config, const char *name, const char *type);
```

Optional:

```
esp_err_t esp_rmaker_node_add_fw_version(const esp_rmaker_node_t *node, const char *fw_version);
esp_err_t esp_rmaker_node_add_model(const esp_rmaker_node_t *node, const char *model);

```

- `esp_rmaker_init()` changed to `esp_rmaker_node_init()`.
- Init function now returns a node handle instead of error code.
- Node Info is no more part of the config structure. The mandatory fields, "name" and "type" are directly passed as strings during initialization.
- Model and fw version are set internally using the project name and version build variables. They can be overriden using `esp_rmaker_node_add_fw_version/model`.

### Devices

#### Old
```
typedef esp_err_t (*esp_rmaker_param_callback_t)(const char *name, const char *dev_name, esp_rmaker_param_val_t val, void *priv_data);

esp_err_t esp_rmaker_create_device(const char *dev_name, const char *type, esp_rmaker_param_callback_t cb, void *priv_data);
```

#### New
```
typedef esp_err_t (*esp_rmaker_device_write_cb_t)(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
        const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx);
typedef esp_err_t (*esp_rmaker_device_read_cb_t)(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
        void *priv_data, esp_rmaker_read_ctx_t *ctx);

esp_rmaker_device_t *esp_rmaker_device_create(const char *dev_name, const char *type, void *priv_data);
esp_err_t esp_rmaker_device_add_cb(const esp_rmaker_device_t *device, esp_rmaker_device_write_cb_t write_cb, esp_rmaker_device_read_cb_t read_cb);
esp_err_t esp_rmaker_node_add_device(const esp_rmaker_node_t *node, const esp_rmaker_device_t *device);
esp_err_t esp_rmaker_node_remove_device(const esp_rmaker_node_t *node, const esp_rmaker_device_t *device);
```

- `esp_rmaker_create_device()` changed to `esp_rmaker_device_create()`. It returns a device handle which has to be used for all further operations.
- The callback has changed such that it now gets the device and param handle, rather than the names. A read callback has also been introduced for future use. It can be kept NULL.
- The callbacks have a new "context" which can have additional information related to that callback, populated by the rainmaker core.
- After creating the device, it has to be added to the node explicitly using `esp_rmaker_node_add_device()`.
- Device can be removed using `esp_rmaker_node_remove_device()`. This may be required for bridges.

### Parameters

#### Old
```
esp_err_t esp_rmaker_device_add_param(const char *dev_name, const char *param_name,
        esp_rmaker_param_val_t val, uint8_t properties);
esp_rmaker_param_add_type(const char *dev_name, const char *param_name, const char* type);
esp_err_t esp_rmaker_update_param(const char *dev_name, const char *param_name, esp_rmaker_param_val_t val);
```

#### New
```
esp_rmaker_param_t *esp_rmaker_param_create(const char *param_name, const char *type,
        esp_rmaker_param_val_t val, uint8_t properties);
esp_err_t esp_rmaker_device_add_param(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param);
esp_err_t esp_rmaker_param_update_and_report(const esp_rmaker_param_t *param, esp_rmaker_param_val_t val);


```

- New API `esp_rmaker_param_create()` introduced to create a parameter. It returns a param handle which has to be used for all further operations.
- `esp_rmaker_device_add_param()` modified to accept the device and param handles.
- `esp_rmaker_param_add_type()` removed because the type is now included in `esp_rmaker_param_create()`
- `esp_rmaker_update_param()` changed to `esp_rmaker_param_update_and_report()`. It now accepts param handle, instead of device and parameter names.
