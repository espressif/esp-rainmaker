# Matter + Rainmaker Controller With Touchscreen Example

- This example demonstrates a Matter + RainMaker Controller which running on [ESP32-S3-BOX](https://github.com/espressif/esp-box) or [ESP32-S3-LCD-EV-BOARD](https://github.com/espressif/esp-dev-kits/tree/master/esp32-s3-lcd-ev-board). Matter is used for commissioning (also known as Wi-Fi provisioning) and local control, whereas RainMaker is used for remote control.
- Thread Border Router support by integrating an ESP32-H2.

## Usage

### Setting [prerequisites](../README.md#prerequisites) and [environment](../README.md#setting-up-the-environment)

### [Claiming device certificates](../README.md#claiming-device-certificates)

### [Generating the factory nvs binary](../README.md#generating-the-factory-nvs-binary)

### Building the example (without OpenThread Border Router)

```
$ idf.py set-target esp32s3
$ idf.py build
$ idf.py flash monitor
```
> Please check the board selection (default ESP32-S3-BOX-3) in one of the ways:
> - Use `idf.py menuconfig`->`HMI Board Config`->`Selece BSP board` to select board.
> - Add only one board config in '[sdkconfig.defaults](matter_controller_with_touchscreen/sdkconfig.defaults)'. e.g. 'CONFIG_BSP_BOARD_ESP32_S3_LCD_EV_BOARD=y' to select ESP32-S3-LCD-EV-BOARD as board. 'CONFIG_BSP_BOARD_ESP32_S3_BOX_3=y' to select ESP32-S3-BOX-3 as board.
>> - Please use ESP-IDF [v5.2](https://github.com/espressif/esp-idf/tree/v5.2).
>> - If the board is ESP32-S3-LCD-EV-BOARD, set-target command should be `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.lcdboard" set-target esp32s3`, remember to select board at the same time.

### Building the example (with OpenThread Border Router)

Build ot_rcp example in `${IDF_PATH}/examples/openthread/ot_rcp`.

Prepare a ESP32-H2 board (flashed ot_rcp example) to connect ESP32-S3-BOX / ESP32-S3-BOX-3 / S3-LCD-EV-BOARD via UART.

| BOX / BOX-3 | ESP32-H2 |
|------------ |----------|
| GND | GND |
| 3V3 | 3V3 |
| GPIO41 | RX |
| GPIO38 | TX |

| S3-LCD-EV-BOARD | ESP32-H2 |
|-----------------|----------|
| GND | GND |
| 3V3 | 3V3 |
| GPIO0 | RX |
| GPIO4 | TX |

Build the example with the sdkconfig file 'sdkconfig.defaults.otbr' to enable the OTBR feature on the controller.

```
$ idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.otbr" set-target esp32s3
$ idf.py build
$ idf.py flash monitor
```
> If the board is ESP32-S3-LCD-EV-BOARD, set-target command should be `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.lcdboard;sdkconfig.defaults.otbr" set-target esp32s3`, remember to check the board selection (default ESP32-S3-BOX-3) at the same time.

> Note: Thread Border Router is not initialized until the Controller has been commissioned and obtained an IP address.

### Commissioning

To commission the Controller with touchscreen, scan the QR Code shown on the screen using ESP RainMaker app.
- After commissioning successfully, the Controller could control the other devices (supporting On/Off cluster server) in the same fabric locally by clicking the icon on the screen.
- Besides the [Matter + Rainmaker light](../matter_light/), a Matter-only light at '$ESP_MATTER_PATH/examples/light' can also be commissioned by using a similar method. The QR code for Matter-only light is on the [website](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html#commissioning-and-control).
- Long click the BOOT button will reset factory, then the device can be commissioned again.

> Please refer to the [README in the parent folder](../README.md) for instructions.


## Fetching the details of the devices commissioned in the RainMaker Fabric:

```
$ cd $RMAKER_PATH/cli
$ ./rainmaker.py login --email <rainmaker-account-email>
$ ./rainmaker.py getparams <rainmaker-node-id-of-controller>
```
**Note**:
1) Please use the same account credentials that are used to claim the controller and signed into the Rainmaker app.
2) Get the rainmaker-node-id-of-controller by logging into [ESP Rainmaker Dashboard](https://dashboard.rainmaker.espressif.com/login).

## Controlling the devices remotely using rainmaker cli:

```
$ cd $RMAKER_PATH/cli
$ ./rainmaker.py login --email <rainmaker-account-email>
```
_Toggle command:_
```
$ ./rainmaker.py setparams --data '{"Matter-Controller":{"Matter-Devices":{"matter-nodes":[{"matter-node-id":"<matter-node-id-of-device>","endpoints":[{"endpoint-id":"0x1","clusters":[{"cluster-id":"0x6","commands":[{"command-id":"0x2"}]}]}]}]}}}' <rainmaker-node-id-of-controller>
```
_Set Brightness command:_
```
$ ./rainmaker.py setparams --data '{"Matter-Controller":{"Matter-Devices":{"matter-nodes":[{"matter-node-id":"<matter-node-id-of-device>","endpoints":[{"endpoint-id":"0x1","clusters":[{"cluster-id":"0x8","commands":[{"command-id":"0x0","data":{"0:U8": 10, "1:U16": 0, "2:U8": 0, "3:U8": 0}}]}]}]}]}}}' <rainmaker-node-id-of-controller>
```
**Note**:
1) Get the matter-node-id-of-device from the details reported by the controller using getparams command.
