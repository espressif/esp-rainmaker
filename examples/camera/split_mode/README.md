# Split Mode Camera Example

This pair of examples demonstrates a **two-device split architecture** for ESP RainMaker Camera, where signaling and media streaming are separated across two processors for optimal power efficiency.

## Architecture Overview

The split mode consists of two separate firmware images:

### 1. **rmaker_split_camera** (ESP32-C6)
- **Role**: ESP RainMaker device with KVS signaling integration
- **Responsibilities**:
  - AWS KVS WebRTC signaling
  - ESP RainMaker device management
  - Bridge communication with media adapter
  - Always-on connectivity for instant responsiveness

### 2. **media_adapter** (ESP32-P4)
- **Role**: Media streaming device
- **Implementation**: Uses the `streaming_only` example from `${KVS_SDK_PATH}/esp_port/examples/streaming_only`
- **Responsibilities**:
  - Video/audio capture and encoding
  - WebRTC media streaming
  - Power-optimized operation (sleeps when not streaming)
  - Receives signaling commands via bridge from rmaker_split_camera

## Hardware Requirements

- **ESP32-P4 Function EV Board** (required)
  - Contains both ESP32-P4 and ESP32-C6 processors
  - Built-in camera support
  - SDIO communication between processors

## System Architecture

```
┌─────────────────┐      SDIO Bridge     ┌─────────────────┐
│    ESP32-C6     │◄────────────────────►│    ESP32-P4     │
│  (rmaker_camera)│      Communication   │ (media_adapter) │
│                 │                      │                 │
│ ┌─────────────┐ │                      │ ┌─────────────┐ │
│ │ ESP         │ │                      │ │ H.264       │ │
│ │ RainMaker   │ │                      │ │ Encoder     │ │
│ │             │ │                      │ │             │ │
│ │ AWS KVS     │ │                      │ │ Camera      │ │
│ │ Signaling   │ │                      │ │ Interface   │ │
│ └─────────────┘ │                      │ └─────────────┘ │
└─────────────────┘                      └─────────────────┘
        ▲                                        ▲
        │                                        │
        ▼                                        ▼
  Internet/AWS                              Video/Audio
    (Signaling)                             Hardware
```

## Quick Start

### Prerequisites

- IDF version: release/v5.5 (v5.5.x)
- [ESP32-P4 Function EV Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html)
- Amazon Kinesis Video Streams WebRTC SDK repository: Clone the `beta-reference-esp-port` branch

  ```bash
  git clone --recursive --single-branch --branch beta-reference-esp-port git@github.com:awslabs/amazon-kinesis-video-streams-webrtc-sdk-c.git amazon-kinesis-video-streams-webrtc-sdk-c
  export KVS_SDK_PATH=/path/to/amazon-kinesis-video-streams-webrtc-sdk-c
  ```

- ESP RainMaker App (iOS/Android) with KVS streaming

### Build and Flash Instructions

⚠️ **Important**: This requires **TWO separate firmware flashes** on the same ESP32-P4 Function EV Board.

#### Step 1: Flash rmaker_split_camera (ESP32-C6)

This handles AWS KVS signaling and ESP RainMaker integration.

```bash
cd split_mode/rmaker_split_camera
idf.py set-target esp32c6
idf.py build
idf.py -p [PORT] flash monitor
```

**Note**: ESP32-C6 does not have an onboard UART port. You will need to use ESP-Prog or any other JTAG.

#### Step 2: Flash media_adapter (ESP32-P4)

This handles video/audio streaming. The firmware is the `streaming_only` example from the KVS SDK.

```bash
cd ${KVS_SDK_PATH}/esp_port/examples/streaming_only
idf.py set-target esp32p4
idf.py menuconfig
# Go to Component config -> ESP System Settings -> Channel for console output
# (X) USB Serial/JTAG Controller # For ESP32-P4 Function_EV_Board V1.2 OR V1.5
# (X) Default: UART0 # For ESP32-P4 Function_EV_Board V1.4
idf.py build
idf.py -p [PORT] flash monitor
```

**Note**: If the console selection is wrong, you will only see the initial bootloader logs. Please change the console as instructed above and reflash the app to see the complete logs.

### Device Configuration

After flashing both firmwares:

1. Use the ESP RainMaker phone app to provision Wi-Fi credentials on the ESP32-C6 (rmaker_split_camera)
2. The camera device should show up in the RainMaker app
3. You can start streaming from the device details page to view the camera feed

### Deep Sleep Feature

 - ESP32-P4 will automatically go into deep sleep once it detects no streaming activity for some time
  - Alternatively, on ESP32-P4's console, simply type `deep-sleep` command. This will put ESP32-P4 into deep sleep
- Start streaming from the ESP RainMaker app (Android/iOS)
- You will see that P4 boots up, and the streaming session is established

## Directory Structure

```
split_mode/
├── README.md                 # This file - overview of split mode
├── rmaker_split_camera/      # ESP32-C6 firmware (signaling + RainMaker)
│   ├── README.md             # Detailed instructions for rmaker_split_camera
│   ├── main/                 # Main application code
│   └── ...
└── media_adapter/            # Documentation for ESP32-P4 firmware
    └── README.md             # Instructions pointing to streaming_only example
                              # Actual firmware: ${KVS_SDK_PATH}/esp_port/examples/streaming_only
```

## Related Documentation

- [rmaker_split_camera README](rmaker_split_camera/README.md) - Detailed setup for ESP32-C6 signaling device
- [media_adapter README](media_adapter/README.md) - Detailed setup for ESP32-P4 streaming device
- [Standalone Mode](../standalone/README.md) - Single-device alternative

## Key Benefits

- **Power Efficiency**: ESP32-P4 can sleep when not streaming, saving significant power
- **Always-On Connectivity**: ESP32-C6 maintains AWS KVS connection 24/7
- **Instant Wake-up**: Shared network stack enables immediate wake-up of ESP32-P4
- **Separation of Concerns**: Signaling and media handling are cleanly separated
