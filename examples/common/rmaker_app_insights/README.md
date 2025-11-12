# RainMaker App Insights Component

[![Component Registry](https://components.espressif.com/components/espressif/rmaker_app_insights/badge.svg)](https://components.espressif.com/components/espressif/rmaker_app_insights)

An application insights and diagnostics helper component for ESP RainMaker applications that provides system monitoring and debugging capabilities.

## Features

- **System Monitoring**: Monitor system health and performance metrics
- **Diagnostics**: Built-in diagnostic capabilities for troubleshooting
- **ESP Insights Integration**: Seamless integration with ESP Insights framework
- **RainMaker Compatibility**: Designed specifically for ESP RainMaker applications

## Usage

Include this component in your ESP-IDF project to add insights and diagnostics functionality to your ESP RainMaker application.

```c
#include "app_insights.h"

// Initialize insights
app_insights_init();
```

## Requirements

- ESP-IDF 5.1 or later
- ESP RainMaker framework
- ESP Insights framework
