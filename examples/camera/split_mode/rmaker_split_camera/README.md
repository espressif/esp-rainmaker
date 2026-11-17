# RainMaker Camera - Split Mode (ESP32-C6 / ESP32-C5)

This example demonstrates the **signaling-only** part of the ESP RainMaker Camera in split mode architecture. This firmware runs on the **ESP32-C6 or ESP32-C5** co-processor and handles AWS KVS WebRTC signaling along with ESP RainMaker device management.

The **media_adapter** firmware (from `split_mode/media_adapter/`) must be flashed on ESP32-P4 to handle the media streaming part.

> **Note — no OTA in split mode:** the co-processor app is ~2.2 MB (it also carries the esp_hosted co-processor firmware), so two OTA slots don't fit in 4 MB flash. The co-processor uses a single `factory` app partition, and RainMaker OTA is unavailable for the split camera. Update it by re-flashing over serial.

## Prerequisites

- IDF version: v5.5.5 or later on v5.5, v6.0.3 or later on v6.0, or v6.1 and later (CI builds with v5.5.5). The co-processor runs esp_hosted 3.x, whose SDIO software aggregation sends more than 4092 bytes at a time. Older IDF versions cap an SDIO send at 4092 bytes, and esp_hosted's build-time check stops the build. Patch such an IDF with `python managed_components/espressif__esp_hosted/tools/eh.py patch-idf`, which changes nothing when the IDF already has the fix.
- [ESP32-P4-Function Ev Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html)
- Amazon Kinesis Video Streams WebRTC SDK C repository: Please clone https://github.com/espressif/esp-port-for-amazon-kvs-sdk
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
 - For host-driven claiming OR for production use-case, please follow camera options in `esp-rainmaker-cli` and `esp-rainmaker-admin-cli` respectively. Certificates must be flashed on the co-processor (`fctry` at `0x3FA000`), e.g. `esp-rainmaker-cli claim --type camera --addr 0x3FA000 <C6/C5 port>`.

## BUILD

- Set up the KVS_SDK_PATH environment variable with clone of the Amazon KVS WebRTC SDK from [here](https://github.com/espressif/esp-port-for-amazon-kvs-sdk) with `--recursive` option:

```bash
    git clone --recursive git@github.com:espressif/esp-port-for-amazon-kvs-sdk.git esp-port-for-amazon-kvs-sdk
    export KVS_SDK_PATH=/path/to/esp-port-for-amazon-kvs-sdk
```

**__NOTE__**:
  1. Confirm that you cloned the default branch
  2. If you missed the `--recursive` option during cloning, run `git submodule update --init --recursive`

- Go to the example directory:
```bash
cd esp-rainmaker/examples/camera/split_mode/rmaker_split_camera
```

- Build the C6 firmware with the pair overlay for your host board. Pass the
  overlay chain in the **same** `set-target` invocation, otherwise the board's
  choice symbols lose to the already-generated sdkconfig:

```bash
# ESP32-P4-EYE (on-board C6):
idf.py -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32c6;sdkconfig.defaults.p4_eye_pair.esp32c6' set-target esp32c6 build

# ESP32-P4 Function EV Board (C6 on the EV header):
idf.py -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32c6;sdkconfig.defaults.evboard_pair.esp32c6' set-target esp32c6 build

# ESP32-P4 + ESP32-C5 core board:
idf.py -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32c5;sdkconfig.defaults.p4_c5_core_board.esp32c5' set-target esp32c5 build
```

The C5 build enables host deep sleep with CP light sleep (esp_hosted's
`host_ps` feature). Its Wi-Fi init settings match the KVS SDK's
`signaling_only` C5 build, and `streaming_only`'s core-board overlay is aligned
to them. Keep the two in step: if the P4's `esp_wifi_init()` config differs
after a deep-sleep wake, hosted re-initialises Wi-Fi and drops the session.

The C5 on the core board has its own USB-UART port, so flash it directly. The
core-board `streaming_only` overlay keeps the P4's slave flasher off, since it
would reset the C5 on every P4 wake.

The C5 build has no reset button. The button must wake the C5 from light
sleep, and only GPIO 0-6 can do that on the C5. The core board's boot button
is on GPIO 28, so the example logs a warning and runs without it. To reset
the device, use the factory reset in the ESP RainMaker phone app (the example
enables the RainMaker system service), or wire a button to GPIO 0-6 and set
`CONFIG_EXAMPLE_BOARD_BUTTON_GPIO`. The `reset-to-factory` console command
works only until the first host wake: esp_hosted's console restart drops it
until rmaker_common 1.13.0.

*__NOTE__*:
- ESP32-C6 on the EV / P4-EYE boards does not have an onboard UART port. You will need to use ESP-Prog or any other JTAG.
- Use following Pin configuration:

| ESP32-C6 (J2/Prog-C6) | ESP-Prog |
|----------|----------|
| IO0      | IO9      |
| TX0      | TXD0     |
| RX0      | RXD0     |
| EN       | EN       |
| GND      | GND      |

- Flash and monitor:
```bash
idf.py -p [PORT] flash monitor
```

- Build and flash the streaming_only example from the KVS SDK on ESP32-P4,
  using the board overlay for your P4 board (see that example for the full
  board list). Pass the overlay chain in the same `set-target` invocation:
```bash
  cd ${KVS_SDK_PATH}/examples/streaming_only
  # e.g. ESP32-P4-EYE:
  idf.py -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32p4;sdkconfig.defaults.p4_eye.esp32p4' set-target esp32p4
  # e.g. ESP32-P4 + ESP32-C5 core board, with deep sleep:
  idf.py -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32p4;sdkconfig.defaults.p4_c5_core_board.esp32p4;sdkconfig.defaults.powersave' set-target esp32p4
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
2. Once done, the camera device should show up in the RainMaker app
3. You can start streaming from the device details page to view the camera feed.

### Trying out Deep Sleep feature
 - Deep sleep needs the ESP32-C5 co-processor, which wakes the ESP32-P4 on a
   viewer's offer. The ESP32-C6 builds have no host wake, so a P4 that sleeps
   there does not wake up again.
 - Build `streaming_only` with `sdkconfig.defaults.powersave`, as in the C5
   command above. Without it the P4 has no idle timer and no `deep-sleep` command.
 - ESP32-P4 will automatically go into deep sleep once it detects no streaming activity for some time
  - Alternatively, on ESP32-P4's console, simply type `deep-sleep` command. This will put ESP32-P4 into deep sleep
 - Start streaming from the ESP RainMaker app (Android/iOS)
 - You will see that, P4 boots up, and the streaming session is established
