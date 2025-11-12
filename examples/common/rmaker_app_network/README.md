# RainMaker App Network Component

[![Component Registry](https://components.espressif.com/components/espressif/rmaker_app_network/badge.svg)](https://components.espressif.com/components/espressif/rmaker_app_network)

A network connectivity helper component for ESP RainMaker applications that provides unified WiFi and Thread networking functionality.

## Features

- **WiFi Management**: Easy WiFi connection and configuration
- **Thread Support**: Thread network connectivity for mesh applications
- **Unified API**: Single interface for different network protocols
- **ESP RainMaker Integration**: Seamless integration with RainMaker framework

## Usage

Include this component in your ESP-IDF project to add network connectivity capabilities to your ESP RainMaker application.

```c
#include "app_network.h"

// Initialize network
app_network_init();
...
...
app_network_start(POP_TYPE_RANDOM)
```

## Requirements

- ESP-IDF 5.1 or later
- ESP RainMaker framework
