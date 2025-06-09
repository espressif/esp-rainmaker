# Media Adapter - Split Mode (ESP32-P4)

**Power-optimized media streaming device** - This firmware handles only media streaming while the separate `rmaker_camera` firmware (on ESP32-C6) manages AWS KVS communication and ESP RainMaker integration.

## Implementation

The media adapter firmware is the **streaming_only** example from the Amazon Kinesis Video Streams WebRTC SDK.

**Location**: `${KVS_SDK_PATH}/esp_port/examples/streaming_only`

This is not a separate ESP RainMaker example - it's the standard KVS SDK streaming_only example that works in conjunction with the `rmaker_camera` firmware.

## Hardware Requirements

- **ESP32-P4 Function EV Board** (required - contains both P4 + C6 processors)
- **Camera**: OV2640, OV3660, OV5640, etc. (built-in on Function EV Board)

## Prerequisites

- IDF version: release/v5.5 (v5.5.x)
- Amazon Kinesis Video Streams WebRTC SDK: Clone the `beta-reference-esp-port` branch

  ```bash
  git clone --recursive --single-branch --branch beta-reference-esp-port git@github.com:awslabs/amazon-kinesis-video-streams-webrtc-sdk-c.git amazon-kinesis-video-streams-webrtc-sdk-c
  export KVS_SDK_PATH=/path/to/amazon-kinesis-video-streams-webrtc-sdk-c
  ```

## Build and Flash

⚠️ **Important**: Flash `rmaker_camera` firmware on ESP32-C6 first (see [rmaker_camera README](../rmaker_camera/README.md)).

```bash
cd ${KVS_SDK_PATH}/esp_port/examples/streaming_only
idf.py set-target esp32p4

# Configure console output (required for different board versions)
idf.py menuconfig
# Go to Component config -> ESP System Settings -> Channel for console output
# (X) USB Serial/JTAG Controller # For ESP32-P4 Function_EV_Board V1.2 OR V1.5
# (X) Default: UART0 # For ESP32-P4 Function_EV_Board V1.4

idf.py build
idf.py -p [PORT] flash monitor
```

**Note**: If the console selection is wrong, you will only see the initial bootloader logs. Change the console as instructed above and reflash.

## Deep Sleep Feature

- ESP32-P4 will automatically go into deep sleep once it detects no streaming activity for some time
- Alternatively, on ESP32-P4's console, type `deep-sleep` command
- Start streaming from the ESP RainMaker app to wake up ESP32-P4 and establish streaming session

## Related Documentation

- [rmaker_camera README](../rmaker_camera/README.md) - Partner device documentation
- [Split Mode Overview](../README.md) - Complete split mode architecture
- [streaming_only README](../../../../amazon-kinesis-video-streams-webrtc-sdk-c/esp_port/examples/streaming_only/README.md) - Full documentation of the streaming_only example
