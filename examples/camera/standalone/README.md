# RainMaker Camera

This example demonstrates how to build a smart camera using ESP chipsets with AWS Kinesis Video Streams (KVS) WebRTC SDK integration and ESP RainMaker for device management.

 - The purpose of this example is to showcase the Real-time video streaming over WebRTC with the help of the device management via ESP RainMaker

## Prerequisites

- IDF version: release/v5.5 (v5.5.x)
- ESP32-P4, ESP-S3, or other ESP32 variants
    - We recommend using ESP32-P4 or ESP-S3 as they have better camera support and hence live streaming is possible
    - For other chipsets, file based streaming is provided
- Tested on the following Dev boards:
    1. [ESP32-P4-Function Ev Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html)
    2. [ESP-S3-EYE](https://github.com/espressif/esp-who/blob/master/docs/en/get-started/ESP32-S3-EYE_Getting_Started_Guide.md)
- Amazon Kinesis Video Streams WebRTC SDK C repository: Please clone the `beta-reference-esp-port` branch of https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/tree/beta-reference-esp-port
- ESP RainMaker App (iOS/Android) with KVS streaming

## Setup ESP-IDF

Please follow the [official guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/) to install and setup the IDF environment:

Main steps involved form Mac/Linux after downloading/cloning the SDK are

```bash
   ./install.sh # Installs the essential tools
   . $IDF_PATH/export.sh # Sets up the environment
```
More comprehensive documentation for setup:
 - [MacOS/Linux](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/linux-macos-setup.html)
 - [Windows](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/windows-setup.html)

## Claiming the Device
 - The firmware supports self-claiming and assisted claiming for seamless experience. So no additional configuration is required while using with public RainMaker.
 - For host-driven claiming OR for production use-case, please follow camera options in `esp-rainmaker-cli` and `esp-rainmaker-admin-cli` respectively.

## BUILD

- Set up the KVS_SDK_PATH environment variable with clone of the Amazon KVS WebRTC SDK from [here](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/tree/beta-reference-esp-port) with `--recursive` option:

```bash
    git clone --recursive --single-branch --branch beta-reference-esp-port git@github.com:awslabs/amazon-kinesis-video-streams-webrtc-sdk-c.git amazon-kinesis-video-streams-webrtc-sdk-c
    export KVS_SDK_PATH=/path/to/amazon-kinesis-video-streams-webrtc-sdk-c
```

*__NOTE__*:
  1. Confirm that you cloned the `beta-reference-esp-port` branch
  2. If you missed the `--recursive` option during cloning, run `git submodule update --init --recursive`

Go to the example directory and follow the steps below:
```bash
    cd esp-rainmaker/examples/camera/standalone
    idf.py set-target [esp32p4/esp32s3/esp32]
```

- Different Development boards have different options for CONSOLE and LOGs
- You may want to do menuconfig and change it as per your board

```bash
    idf.py menuconfig
    # Go to Component config -> ESP System Settings -> Channel for console output
    # (X) USB Serial/JTAG Controller # For ESP32-P4 Function_EV_Board V1.2 OR V1.5
    # (X) Default: UART0 # For ESP32-P4 Function_EV_Board V1.4
```
- If the console selection is wrong, you will only see the initial bootloader logs. Please change the console as instructed above and reflash the app to see the complete logs.


- Build and flash the example
```bash
    idf.py build
    idf.py -p [PORT] flash monitor
```

*__NOTE__*:
- While using P4+C6 setup, please build and flash the network_adapter example from `${KVS_SDK_PATH}/esp_port/examples/network_adapter` on ESP32-C6.
- ESP32-C6 does not have an onboard UART port. You will need to use [ESP-Prog](https://docs.espressif.com/projects/esp-iot-solution/en/latest/hw-reference/ESP-Prog_guide.html) board or any other JTAG.
- Use following Pin Connections:

| ESP32-C6 (J2/Prog-C6) | ESP-Prog |
|----------|----------|
| IO0      | IO9      |
| TX0      | TXD0     |
| RX0      | RXD0     |
| EN       | EN       |
| GND      | GND      |

```bash
    cd ${KVS_SDK_PATH}/esp_port/examples/network_adapter
    idf.py set-target esp32c6
    idf.py build
    idf.py -p [ESP32-C6-PORT] flash monitor
```

## Device Configuration

After flashing the firmware, configure the device:

1. Use the ESP RainMaker phone app to provision Wi-Fi credentials. The step also involves associating the user with the device
2. Once done, thhe camera device should show up in the RainMaker app
3. You can start streaming from the device details page to view the camera feed.
