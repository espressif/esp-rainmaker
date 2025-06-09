# rmaker_camera Component

Shared ESP RainMaker component providing WebRTC camera functionality for ESP RainMaker camera examples. Handles RainMaker device creation, AWS credentials management, and WebRTC initialization.

## Features

- **Device Creation**: `esp_rmaker_camera_device_create()` - Creates camera device with name and channel parameters
- **WebRTC Integration**: `rmaker_webrtc_camera_init()` - Initializes WebRTC stack with KVS signaling
- **Credentials Management**: Automatic AWS credentials fetching using RainMaker core functions
- **Power Management**: Sleep commands for split-mode architectures

## Usage

```c
#include "rmaker_camera.h"

/* Create camera device */
esp_rmaker_device_t *device = esp_rmaker_camera_device_create("ESP32 Camera", NULL);

/* Initialize WebRTC */
rmaker_webrtc_camera_config_t config = {
    .peer_connection_if = kvs_peer_connection_if_get(),
    .video_capture = media_stream_get_video_capture_if(),
    .audio_capture = media_stream_get_audio_capture_if(),
    .init_callback = NULL,
    .init_callback_user_data = NULL,
};
rmaker_webrtc_camera_init(&config);
```

## API

See `include/rmaker_camera.h` for complete API documentation.

## Supported Modes

- **Standalone**: Complete WebRTC with media streaming on single device
- **Split Mode**: Signaling-only for split architecture deployments
