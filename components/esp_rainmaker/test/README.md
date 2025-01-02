# esp_rainmaker unit tests

Please take a look at how to build, flash, and run [esp-idf unit tests](https://github.com/espressif/esp-idf/tree/master/tools/unit-test-app#unit-test-app).

Follow the steps mentioned below to unit test the esp_rainmaker 

* Change to the unit test app directory
```
cd $IDF_PATH/tools/unit-test-app
```

* Set RMAKER_PATH to esp-rainmaker directory
* Note: This is cloned `espressif/esp-rainmaker` directory and not its internal component `esp_rainmaker`
```
export RMAKER_PATH=/path/to/esp-rainmaker
```

* Add following line to partition table csv file
```
fctry, data, nvs, , 0x6000
```

* Copy following contents into `sdkconfig.defaults`
```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partition_table_unit_test_app.csv"

# mbedtls
CONFIG_MBEDTLS_DYNAMIC_BUFFER=y
CONFIG_MBEDTLS_DYNAMIC_FREE_PEER_CERT=y
CONFIG_MBEDTLS_DYNAMIC_FREE_CONFIG_DATA=y
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_CMN=y

# For BLE Provisioning using NimBLE stack (Not applicable for ESP32-S2)
CONFIG_BT_ENABLED=y
CONFIG_BTDM_CTRL_MODE_BLE_ONLY=y
CONFIG_BT_NIMBLE_ENABLED=y

# Temporary Fix for Timer Overflows
CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH=3120

# For additional security on reset to factory
CONFIG_ESP_RMAKER_USER_ID_CHECK=y

# Secure Local Control
CONFIG_ESP_RMAKER_LOCAL_CTRL_AUTO_ENABLE=y
#CONFIG_ESP_RMAKER_LOCAL_CTRL_ENABLE is deprecated but will continue to work
CONFIG_ESP_RMAKER_LOCAL_CTRL_SECURITY_1=y

# Application Rollback
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y

# If ESP-Insights is enabled, we need MQTT transport selected
# Takes out manual efforts to enable this option
CONFIG_ESP_INSIGHTS_TRANSPORT_MQTT=y

```
* Append `/path/to/esp-rainmaker/components` and `/path/to/esp-rainmaker/examples/common` directory to `EXTRA_COMPONENT_DIRS` in `CMakeLists.txt`

## Build, flash and run tests
```
# Clean any previous configuration and builds
rm -r sdkconfig build

# Set the target
idf.py set-target esp32s3

# Building the firmware
idf.py -T esp_rainmaker build

# Flash and run the test cases
idf.py -p <serial-port> -T esp_rainmaker flash monitor
```
