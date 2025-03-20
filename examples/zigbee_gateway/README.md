# Gateway Example

## What to expect in this example?

The example demonstrates the functionality of a Rainmaker based Zigbee gateway device. It could be provisioned via the standard Rainmaker provisioning flow, and then acts as a Zigbee Gateway, each joined Zigbee device will be mapped as Rainmaker device, so it can be controlled from Rainmaker App and cloud.

It currently supports two methods to add Zigbee devices:
- Network steering using the pre-shared link key "ZigBeeAlliance09"
- Network steering using the install code mode

## Hardware Platforms

### Wi-Fi based ESP Zigbee Gateway

The Wi-Fi based ESP Zigbee Gateway consists of two SoCs:

* An ESP32 series Wi-Fi SoC (ESP32, ESP32-C, ESP32-S, etc) loaded with ESP Zigbbe Gateway and Zigbee Stack.
* An ESP32-H 802.15.4 SoC loaded with OpenThread RCP.

The following boards are available for this example:
- [ESP Zigbee gateway board](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_gateway#hardware-platforms)
- [M5Stack CoreS3](https://shop.m5stack.com/products/m5stack-cores3-esp32s3-lotdevelopment-kit) + [Module Gateway H2](https://shop.m5stack.com/products/esp32-h2-thread-zigbee-gateway-module)

## Build and Flash firmware

### Build the RCP firmware

The Zigbee Gateway supports flashing the RCP image from the host SoC.

First build the [ot_rcp](https://github.com/espressif/esp-idf/tree/master/examples/openthread/ot_rcp),`OPENTHREAD_NCP_VENDOR_HOOK` of ot_rcp should be selected via menuconfig.

### Build and Flash Zigbee Gateway Firmware

Follow the ESP RainMaker Documentation [Get Started](https://rainmaker.espressif.com/docs/get-started.html) section to build and flash this firmware. Just note the path of this example.

Build command depends on the board used:
- For ESP Zigbee gateway board:
```
idf.py set-target esp32s3 build
```
- For M5Stack CoreS3 + Module Gateway H2:
```
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.m5stack" set-target esp32s3 build
```

> Note: If failed to flash M5Stack-CoreS3 board, please long press the the bottom RST button.

## Add device

### Supported devices mapping to RainMaker:

- ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID (0x0100)
- ESP_ZB_HA_IAS_ZONE_ID (0x0402, Zone type: Door/Window handle)

After adding device and refreshing RainMaker App, you can see the device in App.

### Add Zigbee device via pre-shared link key

- In the ESP Rainmaker app, use the "Add_zigbee_device" button to open the Zigbee network and add devices. You should see the following log:

```
I (81687) app_wifi: Connected with IP Address:192.168.1.106
I (81687) esp_netif_handlers: sta ip: 192.168.1.106, mask: 255.255.255.0, gw: 192.168.1.1
I (81687) wifi_prov_mgr: STA Got IP
I (81697) app_wifi: Provisioning successful
I (81697) main_task: Returned from app_main()
I (81727) esp_mqtt_glue: AWS PPI: ?Platform=APN3|A0|RM|EX00|RMDev|1x0|240AC41C91F8
I (81727) esp_mqtt_glue: Initialising MQTT
I (81727) esp_rmaker_mqtt_budget: MQTT Budgeting initialised. Default: 100, Max: 1024, Revive count: 1, Revive period: 5
I (81737) esp_rmaker_local: Couldn't find POP in NVS. Generating a new one.
I (81747) esp_rmaker_local: Waiting for Wi-Fi provisioning to finish.
I (81747) esp_mqtt_glue: Connecting to a1p72mufdu6064-ats.iot.us-east-1.amazonaws.com
I (81757) esp_rmaker_core: Waiting for MQTT connection
I (81817) wifi:<ba-add>idx:1 (ifx:0, 24:69:8e:07:04:25), tid:0, ssn:1, winSize:64
I (82927) esp-x509-crt-bundle: Certificate validated
I (84767) esp_mqtt_glue: MQTT Connected
I (84767) esp_rmaker_node_config: Reporting Node Configuration of length 1342 bytes.
I (84777) esp_rmaker_core: Waiting for User Node Association.
I (84777) esp_rmaker_ota_using_topics: Subscribing to: node/XZTfzxp4YfXqLmDwPvNTUi/otaurl
I (84787) esp_rmaker_user_mapping: MQTT Publish: {"node_id":"XZTfzxp4YfXqLmDwPvNTUi","user_id":"0e155512-8946-4be4-a0e7-1e0e74b901c3","secret_key":"180A1405-894A-44F1-A84D-452B42F4FB35","reset":true}
I (85577) esp_rmaker_user_mapping: User Node association message published successfully.
I (85587) esp_rmaker_param: Params MQTT Init done.
I (85597) esp_rmaker_param: Reporting params (init): {"Zigbee_Gateway":{"Add_zigbee_device":""},"Time":{"TZ":"","TZ-POSIX":""},"Schedule":{"Schedules":[]},"Scenes":{"Scenes":[]},"Local Control":{"POP":"e3fe1a33","Type":1}}
I (85607) esp_rmaker_cmd_resp: Enabling Command-Response Module.
I (86347) NimBLE: GAP procedure initiated: stop advertising.
I (86357) NimBLE: GAP procedure initiated: stop advertising.
I (86357) NimBLE: GAP procedure initiated: terminate connection; conn_handle=0 hci_reason=19
E (86397) protocomm_nimble: Error setting advertisement data; rc = 30
I (86407) wifi_prov_mgr: Provisioning stopped
I (86407) wifi_prov_scheme_ble: BTDM memory released
I (86407) esp_rmaker_local: Event 6
I (86407) esp_rmaker_local: Starting ESP Local control with HTTP Transport and security version: 1
I (86437) esp_https_server: Starting server
I (86437) esp_https_server: Server listening on port 8080
I (86437) esp_rmaker_local: esp_local_ctrl service started with name : XZTfzxp4YfXqLmDwPvNTUi
I (89067) esp_rmaker_param: Received params: {"Time":{"TZ":"Asia/Shanghai"}}
I (89067) esp_rmaker_time_service: Received value = Asia/Shanghai for Time - TZ
I (89077) esp_rmaker_time: Time not synchronised yet.
I (89077) esp_rmaker_time: The current time is: Thu Jan  1 08:01:28 1970 +0800[CST], DST: No.
I (89097) esp_rmaker_param: Reporting params: {"Time":{"TZ-POSIX":"CST-8"}}
I (89097) esp_rmaker_param: Reporting params: {"Time":{"TZ":"Asia/Shanghai"}}
I (89787) esp_rmaker_ota_using_topics: Fetching OTA details, if any.
I (100537) esp_rmaker_param: Received params: {"Zigbee_Gateway":{"Add_zigbee_device":true}}
I (100537) esp_app_rainmaker: Received write request via : Cloud
I (100547) esp_app_rainmaker: Received device name = Zigbee_Gateway,parameter name = Add_zigbee_device
I (100557) esp_app_rainmaker: Zigbee network close after 180 seconds
```

> Note: For M5Stack-CoreS3 board, you can also toggle the "Add zigbee device" button on the screen to to open the Zigbee network and add devices.

### Add Zigbee device via install code mode

- Within the ESP RainMaker app, an icon representing the Zigbee gateway can be accessed through a QR code scanning interface. This allows for the input of installation codes and MAC addresses for Zigbee end-devices or routers. The following link [QR Code](https://rainmaker.espressif.com/qrcode.html?data={"install_code":"83FED3407A939723A5C639B26916D505C3B5","MAC_address":"74fa0801a003f784"}) can generate a QR code containing the install code and MAC address. You can modify it according to the actual Zigbee end devices.
- Please ensure that the zigbee end-device's MAC address and install code are correctly included in the QR code.
- After scanning the QR code from a ready-to-join Zigbee device, the Cloud processes the information before passing it on as a JSON string to the Zigbee gateway device. The installation codes and MAC addresses are displayed in the log.
- In order to enable install code feature, navigate to: `idf.py menuconfig --> ESP Rainmaker Zigbee Gateway Example --> Enable zigbee install code`.
```
I (81687) app_wifi: Connected with IP Address:192.168.1.106
I (81687) esp_netif_handlers: sta ip: 192.168.1.106, mask: 255.255.255.0, gw: 192.168.1.1
I (81687) wifi_prov_mgr: STA Got IP
I (81697) app_wifi: Provisioning successful
I (81697) main_task: Returned from app_main()
I (81727) esp_mqtt_glue: AWS PPI: ?Platform=APN3|A0|RM|EX00|RMDev|1x0|240AC41C91F8
I (81727) esp_mqtt_glue: Initialising MQTT
I (81727) esp_rmaker_mqtt_budget: MQTT Budgeting initialised. Default: 100, Max: 1024, Revive count: 1, Revive period: 5
I (81737) esp_rmaker_local: Couldn't find POP in NVS. Generating a new one.
I (81747) esp_rmaker_local: Waiting for Wi-Fi provisioning to finish.
I (81747) esp_mqtt_glue: Connecting to a1p72mufdu6064-ats.iot.us-east-1.amazonaws.com
I (81757) esp_rmaker_core: Waiting for MQTT connection
I (81817) wifi:<ba-add>idx:1 (ifx:0, 24:69:8e:07:04:25), tid:0, ssn:1, winSize:64
I (82927) esp-x509-crt-bundle: Certificate validated
I (84767) esp_mqtt_glue: MQTT Connected
I (84767) esp_rmaker_node_config: Reporting Node Configuration of length 1342 bytes.
I (84777) esp_rmaker_core: Waiting for User Node Association.
I (84777) esp_rmaker_ota_using_topics: Subscribing to: node/XZTfzxp4YfXqLmDwPvNTUi/otaurl
I (84787) esp_rmaker_user_mapping: MQTT Publish: {"node_id":"XZTfzxp4YfXqLmDwPvNTUi","user_id":"0e155512-8946-4be4-a0e7-1e0e74b901c3","secret_key":"180A1405-894A-44F1-A84D-452B42F4FB35","reset":true}
I (85577) esp_rmaker_user_mapping: User Node association message published successfully.
I (85587) esp_rmaker_param: Params MQTT Init done.
I (85597) esp_rmaker_param: Reporting params (init): {"Zigbee_Gateway":{"Add_zigbee_device":""},"Time":{"TZ":"","TZ-POSIX":""},"Schedule":{"Schedules":[]},"Scenes":{"Scenes":[]},"Local Control":{"POP":"e3fe1a33","Type":1}}
I (85607) esp_rmaker_cmd_resp: Enabling Command-Response Module.
I (86347) NimBLE: GAP procedure initiated: stop advertising.
I (86357) NimBLE: GAP procedure initiated: stop advertising.
I (86357) NimBLE: GAP procedure initiated: terminate connection; conn_handle=0 hci_reason=19
E (86397) protocomm_nimble: Error setting advertisement data; rc = 30
I (86407) wifi_prov_mgr: Provisioning stopped
I (86407) wifi_prov_scheme_ble: BTDM memory released
I (86407) esp_rmaker_local: Event 6
I (86407) esp_rmaker_local: Starting ESP Local control with HTTP Transport and security version: 1
I (86437) esp_https_server: Starting server
I (86437) esp_https_server: Server listening on port 8080
I (86437) esp_rmaker_local: esp_local_ctrl service started with name : XZTfzxp4YfXqLmDwPvNTUi
I (89067) esp_rmaker_param: Received params: {"Time":{"TZ":"Asia/Shanghai"}}
I (89067) esp_rmaker_time_service: Received value = Asia/Shanghai for Time - TZ
I (89077) esp_rmaker_time: Time not synchronised yet.
I (89077) esp_rmaker_time: The current time is: Thu Jan  1 08:01:28 1970 +0800[CST], DST: No.
I (89097) esp_rmaker_param: Reporting params: {"Time":{"TZ-POSIX":"CST-8"}}
I (89097) esp_rmaker_param: Reporting params: {"Time":{"TZ":"Asia/Shanghai"}}
I (89787) esp_rmaker_ota_using_topics: Fetching OTA details, if any.
I (100537) esp_rmaker_param: Received params: {"Zigbee_Gateway":{"Add_zigbee_device":"{\"install_code\":\"83FED3407A939723A5C639B26916D505C3B5\",\"MAC_address\":\"74fa0801a003f784\"}"}}
I (100537) app_main: Received write request via : Cloud
I (100547) app_main: Received device name = Zigbee_Gateway,parameter name = Add_zigbee_device
I (100557) app_main: prase zigbee ic success
I (100557) app_main: prase install code:83fed3407a939723a5c639b26916d505 CRC:c3b5
I (100567) app_main: prase MAC address::74fa0801a003f784
I (100577) esp_rmaker_param: Reporting params: {"Zigbee_Gateway":{"Add_zigbee_device":"{"install_code":"83FED3407A939723A5C639B26916D505C3B5","MAC_address":"74fa0801a003f784"}"}}
```

### Reset to Factory

Press and hold the BOOT button for more than 3 seconds to reset the board to factory defaults. You will have to provision the board again to use it.

> Note: For M5Stack-CoreS3, use the button on the touch screen to reset.
