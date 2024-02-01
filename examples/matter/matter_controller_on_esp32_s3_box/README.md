# Matter + Rainmaker Controller On ESP32-S3-BOX Example

## What to expect in this example?

- This demonstrates a Matter + RainMaker Controller which running on [ESP32-S3-BOX](https://github.com/espressif/esp-box). Matter is used for commissioning (also known as Wi-Fi provisioning) and local control, whereas RainMaker is used for remote control.
- Please check the board (ESP32-S3-BOX / ESP32-S3-BOX-3) by `idf.py menuconfig`, default is ESP32-S3-BOX-3.
- To commission the Controller on ESP32-S3-BOX, scan the QR Code shown on the screen using ESP RainMaker app.
- After commissioning successfully, the Controller could control the other devices (supporting On/Off cluster server) in the same fabric locally by clicking the button on the screen.
- The devices in the fabric can also be controlled remotely using the Rainmaker iOS app.
- Besides the [Matter + Rainmaker light](../matter_light/), a Matter-only light at '$ESP_MATTER_PATH/examples/light' can also be commissioned by using a similar method. The QR code for Matter-only light is in the [website](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html#commissioning-and-control).
- Long click the BOOT button (on the upper left side of the screen) will reset factory, then the device can be commissioned again.

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