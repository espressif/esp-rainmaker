# RainMaker Camera - Split Mode (ESP32-C6)

This example demonstrates the **signaling-only** part of the ESP RainMaker Camera in split mode architecture. This firmware runs on **ESP32-C6** and handles AWS KVS WebRTC signaling along with ESP RainMaker device management.

The **media_adapter** firmware (from `split_mode/media_adapter/`) must be flashed on ESP32-P4 to handle the media streaming part.

## Prerequisites

- IDF version: release/v5.5 (v5.5.x)
- [ESP32-P4-Function Ev Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html)
- Amazon Kinesis Video Streams WebRTC SDK C repository: Please clone the `beta-reference-esp-port` branch of https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/tree/beta-reference-esp-port
- ESP RainMaker App (iOS/Android) with KVS streaming

## Setup ESP-IDF

Please follow the [official guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/) to install and setup the IDF environment:

Main steps involved for Mac/Linux after downloading/cloning the SDK are

   ```bash
   ./install.sh # Installs the essential tools
   . $IDF_PATH/export.sh # Sets up the environment
   ```
More comprehensive documentation for setup:
 - [MacOS/Linux](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/linux-macos-setup.html)
 - [Windows](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/windows-setup.html)


## Claiming the Device
 - The firmware supports self-claiming and assisted claiming for seamless experience. So no additional configuration is required while using with public RainMaker.
 - For host-driven claiming OR for production use-case, please follow camera options in `esp-rainmaker-cli` and `esp-rainmaker-admin-cli` respectively. Certificates must be flashed on ESP32-C6.

## BUILD

- Set up the KVS_SDK_PATH environment variable with clone of the Amazon KVS WebRTC SDK from [here](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/tree/beta-reference-esp-port) with `--recursive` option:

```bash
    git clone --recursive --single-branch --branch beta-reference-esp-port git@github.com:awslabs/amazon-kinesis-video-streams-webrtc-sdk-c.git amazon-kinesis-video-streams-webrtc-sdk-c
    export KVS_SDK_PATH=/path/to/amazon-kinesis-video-streams-webrtc-sdk-c
```

**__NOTE__**:
  1. Confirm that you cloned the `beta-reference-esp-port` branch
  2. If you missed the `--recursive` option during cloning, run `git submodule update --init --recursive`

- Go to the example directory and follow the steps below:
```bash
cd esp-rainmaker/examples/camera/split_mode/rmaker_camera
idf.py set-target esp32c6
```

*__NOTE__*:
- ESP32-C6 does not have an onboard UART port. You will need to use ESP-Prog or any other JTAG.
- Use following Pin configuration:

| ESP32-C6 (J2/Prog-C6) | ESP-Prog |
|----------|----------|
| IO0      | IO9      |
| TX0      | TXD0     |
| RX0      | RXD0     |
| EN       | EN       |
| GND      | GND      |

- Build and flash the example
```bash
idf.py build
idf.py -p [PORT] flash monitor
```

- Build and flash the streaming_only example from KVS SDK on ESP32-P4:
```bash
  cd ${KVS_SDK_PATH}/esp_port/examples/streaming_only
  idf.py set-target esp32p4
```

- For ESP32-P4, different versions of the Dev boards have different options for CONSOLE and LOGs
- You may want to do menuconfig and change it as per your board

```bash
  idf.py menuconfig
  # Go to Component config -> ESP System Settings -> Channel for console output
  # (X) USB Serial/JTAG Controller # For ESP32-P4 Function_EV_Board V1.2 OR V1.5
  # (X) Default: UART0 # For ESP32-P4 Function_EV_Board V1.4
```
- If the console selection is wrong, you will only see the initial bootloader logs.
  - Please change the console as instructed above and reflash the app to see the complete logs.

- Now build and flash the `streaming_only` example on ESP32-P4
```bash
idf.py build
idf.py -p [PORT] flash monitor
```

## Device Configuration

After flashing the firmware, configure the device:

1. Use the ESP RainMaker phone app to provision Wi-Fi credentials. The step also involves associating the user with the device
2. Once done, thhe camera device should show up in the RainMaker app
3. You can start streaming from the device details page to view the camera feed.

### Trying out Deep Sleep feature
 - ESP32-P4 will automatically go into deep sleep once it detects no streaming activity for some time
  - Alternatively, on ESP32-P4's console, simply type `deep-sleep` command. This will put ESP32-P4 into deep sleep
 - Start streaming from the ESP RainMaker app (Android/iOS)
 - You will see that, P4 boots up, and the streaming session is established
