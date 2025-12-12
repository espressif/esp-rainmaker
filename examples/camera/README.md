# ESP RainMaker Camera Examples

This directory contains ESP RainMaker camera examples that demonstrate building smart cameras using ESP chipsets with [AWS Kinesis Video Streams](https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/what-is-kvswebrtc.html) (KVS) [WebRTC SDK](https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c/tree/beta-reference-esp-port) integration and ESP RainMaker for device management.

**Key Features of the RainMaker Camera:**
- Complete WebRTC stack with STUN and TURN capabilities
- KVS peer connection with media streaming
- Audio/Video capture and playback interfaces

## Available Examples

### 1. Standalone Mode (`standalone/`)

Complete WebRTC implementation including both signaling and media streaming on a single device.

**Use Case:**
- Single device deployment
- Direct camera streaming with WebRTC
- Faster Peer connection setup

**Supported Devices:**
- ESP32-P4 (FHD live streaming)
- ESP32-S3 (QVGA live streaming)
- Other ESP32 variants (file-based streaming)

[Learn more →](standalone/README.md)

### 2. Split Mode (`split_mode/`)

Split architecture where streaming is handled by a separate MCU and Signaling is handled by the RainMaker Camera.

**Use Case:**
- Distributed architecture (signaling on one device, streaming on another)
- This enables manufactures to swap streaming implementation swiftly
- Power-optimized deployments

**Supported Devices:**
  - ESP32-P4 Function Ev Board
    - ESP32-C6 (signaling side RainMaker integration)
    - ESP32-P4 (streaming side - uses KVS streaming_only example)

[Learn more →](split_mode/README.md)

## Common Component: `rmaker_camera`

Both examples use a shared `rmaker_camera` component located in `components/rmaker_camera/`. The component behavior is controlled via Kconfig:

- `CONFIG_RMAKER_CAMERA_MODE_STANDALONE`: Complete WebRTC with media streaming
- `CONFIG_RMAKER_CAMERA_MODE_SPLIT_MODE`: Signaling-only for split architecture

The mode is set via each example's `sdkconfig.defaults` files, so no manual configuration is needed.

**The main difference in two modes is standalone provides the single board application with instantaneous peer connection, whereas, split mode provides a way to make the media_adapter to be put in deep sleep when not in use and hence targetting power sensitive deployments**

## Prerequisites

- **IDF version**: release/v5.5 (v5.5.x)
- **Amazon Kinesis Video Streams WebRTC SDK**: Clone the `beta-reference-esp-port` branch

  ```bash
  git clone -b beta-reference-esp-port git@github.com:awslabs/amazon-kinesis-video-streams-webrtc-sdk-c.git
  export KVS_SDK_PATH=/path/to/amazon-kinesis-video-streams-webrtc-sdk-c
  ```
- **ESP RainMaker App**: Latest iOS/Android apps with Camera support
  - Android: v3.9.2 or later
  - iOS: v3.5.2 or later

## Getting Started

1. Choose the appropriate example based on your use case:
   - Device for complete experience → **Standalone Mode**
   - Distributed setup with power saving feature → **Split Mode**

2. Follow the detailed instructions in the respective example's README:
   - [Standalone Mode README](standalone/README.md)
   - [Split Mode README](split_mode/README.md)

3. Both examples use the same RainMaker camera component, which automatically adapts based on the configured mode.

## Architecture

### Standalone Mode

**Single Chip Solution**

```
┌────────────────────────────────┐
│   ESP32-S3 (Single Device)     │
│                                │
│  ┌──────────────────────────┐  │
│  │   RainMaker Camera       │  │
│  │   (Standalone Mode)      │  │
│  ├──────────────────────────┤  │
│  │  • Signaling             │  │
│  │  • Media Streaming       │  │
│  │  • Audio/Video Capture   │  │
│  │  • Audio Playback        |  |
│  └──────────────────────────┘  │
└────────────────────────────────┘
```

**Dual Chip Solution**

### Standalone Mode

```
┌────────────────────────────────┐      ┌─────────────────────────────┐
│         ESP32-P4               │ SDIO │         ESP32-C6            │
│                                │◄────►│     (Network Adapter)       │
│  ┌──────────────────────────┐  │      └─────────────────────────────┘
│  │   RainMaker Camera       │  │
│  │   (Standalone Mode)      │  │
│  ├──────────────────────────┤  │
│  │  • Signaling             │  │
│  │  • Media Streaming       │  │
│  │  • Audio/Video Capture   │  │
│  │  • Audio Playback        │  │
│  └──────────────────────────┘  │
└────────────────────────────────┘
```

### Split Mode

```
┌────────────────────────────────┐      ┌─────────────────────────────┐
│         ESP32-C6               │ SDIO │         ESP32-P4            │
│      (RainMaker Camera)        │◄────►│   (Streaming Device)        │
│  ┌──────────────────────────┐  │      ├─────────────────────────────┤
│  │   Camera Split Mode      │  │      │                             │
│  ├──────────────────────────┤  │      │   • Media Streaming         │
│  │  • RainMaker Device Mgmt │  │      │   • Audio/Video Capture     │
│  │  • Signaling             │  │      │   • Audio Playback          │
│  └──────────────────────────┘  │      └─────────────────────────────┘
└────────────────────────────────┘
```
