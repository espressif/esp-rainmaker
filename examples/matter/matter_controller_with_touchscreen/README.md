# Matter + RainMaker Controller With Touchscreen Example

This example brings up a Matter controller with a touchscreen UI and RainMaker cloud connectivity.

- Board selection is done through `esp_board_manager` generated configuration, not `menuconfig` BSP selection.
- Thread Border Router support is available by pairing the display board with an ESP32-H2 RCP. For M5Stack-CoreS3, [Module Gateway H2](https://shop.m5stack.com/products/esp32-h2-thread-zigbee-gateway-module) can be used, or you can buy the whole [M5Stack Thread Border Router](https://shop.m5stack.com/products/m5stack-thread-border-router?variant=47023582281985) set.
- Setup steps for environment, claiming, and factory data are shared with the other Matter + RainMaker examples in the [parent README](../README.md).

## Supported Boards

All of the following criteria must be met:

- The board must appear in the [`esp_board_manager` supported boards list](https://components.espressif.com/components/espressif/esp_board_manager/versions/0.5.8/readme?language=en#supported-boards).
- The board must use an `esp32s3` chip.
- The board must support LCD Touch.

At the time of writing, the supported boards are:

- [ESP32-S3-BOX/BOX-3](https://github.com/espressif/esp-box)
- [M5Stack-CoreS3](https://docs.m5stack.com/en/core/CoreS3)

## Setup

> [!NOTE]
> Require ESP-IDF version `>=5.4.3,!=5.5.0,!=5.5.1` and esp-matter `release/v1.5` branch.

Before building this example, complete these shared setup steps from the [parent README](../README.md):

- [Prerequisites](../README.md#prerequisites)
- [Environment setup](../README.md#setting-up-the-environment)
- [Claiming device certificates](../README.md#claiming-device-certificates)
- [Generating the factory NVS binary](../README.md#generating-the-factory-nvs-binary)

## Build and Flash

### Without OpenThread Border Router

```bash
idf.py set-target esp32s3
idf.py bmgr -l
idf.py bmgr -b <board>
idf.py build
idf.py flash monitor
```

Use the board-manager board name for `<board>`. The `bmgr -l` step prints the available names, for example:

```bash
✅ Found 13 board(s):

ℹ️  Main Boards:
  [1] dual_eyes_board_v1_0
  [2] esp32_c3_lyra
  [3] esp32_c5_spot
  [4] esp32_p4_function_ev
  [5] esp32_s3_korvo2_v3
  [6] esp32_s3_korvo2l
  [7] esp_box_3
  [8] esp_box_lite
  [9] esp_vocat_board_v1_0
  [10] esp_vocat_board_v1_2
  [11] lyrat_mini_v1_1
  [12] m5stack_cores3
  [13] m5stack_tab5

✅ Board listing completed!
```

### With OpenThread Border Router

Prepare an ESP32-H2 board flashed with the `ot_rcp` example and connect it to your chosen display board over UART:

| ESP32-H2 | BOX / BOX-3 | M5Stack-CoreS3 |
| -------- | ----------- | -------------- |
| GND      | GND         | GND            |
| 3V3      | 3V3         | 3V3            |
| RX       | GPIO41      | GPIO17         |
| TX       | GPIO38      | GPIO10         |

First build the [ot_rcp](https://github.com/espressif/esp-idf/tree/master/examples/openthread/ot_rcp).

Then build this example with OTBR defaults enabled:

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.otbr" set-target esp32s3
idf.py bmgr -l
idf.py bmgr -b <board>
idf.py build
idf.py flash monitor
```

> Note: Thread Border Router is not initialized until the Controller has been commissioned and obtained an IP address.

## Commissioning

Commission the controller by scanning the QR code shown on the screen with the ESP RainMaker app.

- After commissioning, the controller can locally control devices in the same fabric that implement the On/Off cluster server by tapping the on-screen icon.
- Besides the [Matter + RainMaker light](../matter_light/), you can also commission the Matter-only light example at `$ESP_MATTER_PATH/examples/light`. The QR code for that example is documented on the [ESP-Matter commissioning page](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html#commissioning-and-control).
- Long-press the `BOOT` button, or use the Reset button in the `About Us` page, to factory reset the controller and recommission it.
- The QR code used to commission this controller is generated as part of the factory data flow described in the [parent README](../README.md#commissioning).

For factory-data generation and QR code details, see the [README in the parent folder](../README.md).

## RainMaker CLI

Use `esp-rainmaker-cli` instead of the legacy `rainmaker.py` script. For installation and current command reference, see the [rainmaker-cli user guide](https://docs.rainmaker.espressif.com/docs/dev/dashboard-cli/rainmaker-cli-user-guide/).

### Matter-Controller node config

```json
{
  "name": "Matter-Controller",
  "params": [
    {
      "data_type": "object",
      "name": "Matter-Devices",
      "properties": ["read"],
      "type": "esp.param.matter-devices"
    },
    {
      "data_type": "string",
      "name": "Matter-Controller-Data-Version",
      "properties": ["read"],
      "type": "esp.param.matter-controller-data-version"
    }
  ],
  "type": "esp.service.matter-controller"
}
```

### Fetch commissioned-device details

```bash
esp-rainmaker-cli login
esp-rainmaker-cli getparams <rainmaker-node-id-of-controller>
```

Notes:

1. Use the same RainMaker account that was used to claim the controller and is signed in to the RainMaker app.
2. Get `<rainmaker-node-id-of-controller>` from the RainMaker app, RainMaker dashboard, or `esp-rainmaker-cli getnodes`.

Example `getparams` output:

```json
{
  "Matter Controller": {
    "Name": "Controller",
    "Power": true
  },
  "Matter-Controller": {
    "Matter-Controller-Data-Version": "1.0.1",
    "Matter-Devices": {
      "d81b1178e484c26d": {
        "enabled": true,
        "endpoints": {
          "0x1": {
            "clusters": {
              "servers": {
                "0x3": {
                  "attributes": {
                    "0x0": 0,
                    "0x1": 0
                  }
                },
                "0x300": {
                  "attributes": {
                    "0x10": 0,
                    "0x2": 0,
                    "0x3": 24939,
                    "0x4": 24701,
                    "0x4001": 2,
                    "0x400a": 24,
                    "0x400b": 0,
                    "0x400c": 65279,
                    "0x400d": 0,
                    "0x4010": "Unhandled",
                    "0x7": 250,
                    "0x8": 2,
                    "0xf": 0
                  }
                },
                "0x4": {
                  "attributes": {
                    "0x0": 128
                  }
                },
                "0x6": {
                  "attributes": {
                    "0x0": true,
                    "0x4000": true,
                    "0x4001": 0,
                    "0x4002": 0,
                    "0x4003": "Unhandled"
                  }
                },
                "0x62": {
                  "attributes": {
                    "0x1": 16,
                    "0x2": "Unhandled"
                  }
                },
                "0x8": {
                  "attributes": {
                    "0x0": 64,
                    "0x1": 0,
                    "0x11": "Unhandled",
                    "0x2": 1,
                    "0x3": 254,
                    "0x4000": 64,
                    "0xf": 0
                  }
                }
              }
            },
            "device_type": "0x10d"
          },
          "0x2": {
            "clusters": {
              "servers": {
                "0x3": {
                  "attributes": {
                    "0x0": 0,
                    "0x1": 0
                  }
                },
                "0x3b": {
                  "attributes": {
                    "0x0": 2,
                    "0x1": 0
                  },
                  "events": {
                    "0x0": false,
                    "0x1": false,
                    "0x2": false,
                    "0x3": false,
                    "0x4": false,
                    "0x5": false,
                    "0x6": false
                  }
                }
              }
            },
            "device_type": "0xf"
          },
          "0x3": {
            "clusters": {
              "clients": ["0x3", "0x6", "0x4"],
              "servers": {
                "0x1e": {
                  "attributes": {
                    "0x0": "Unhandled"
                  }
                },
                "0x3": {
                  "attributes": {
                    "0x0": 0,
                    "0x1": 0
                  }
                },
                "0x4": {
                  "attributes": {
                    "0x0": 128
                  }
                }
              }
            },
            "device_type": "0x103"
          }
        },
        "reachable": true
      }
    }
  },
  "Scenes": {
    "Scenes": []
  },
  "Schedule": {
    "Schedules": []
  },
  "Time": {
    "TZ": "",
    "TZ-POSIX": ""
  }
}
```

### Control devices remotely

```bash
esp-rainmaker-cli login
```

Toggle command:

```bash
esp-rainmaker-cli setparams --data '{"Matter-Controller":{"Matter-Devices":{"matter-nodes":[{"matter-node-id":"<matter-node-id-of-device>","endpoints":[{"endpoint-id":"0x1","clusters":[{"cluster-id":"0x6","commands":[{"command-id":"0x2"}]}]}]}]}}}' <rainmaker-node-id-of-controller>
```

Set brightness command:

```bash
esp-rainmaker-cli setparams --data '{"Matter-Controller":{"Matter-Devices":{"matter-nodes":[{"matter-node-id":"<matter-node-id-of-device>","endpoints":[{"endpoint-id":"0x1","clusters":[{"cluster-id":"0x8","commands":[{"command-id":"0x0","data":{"0:U8": 10, "1:U16": 0, "2:U8": 0, "3:U8": 0}}]}]}]}]}}}' <rainmaker-node-id-of-controller>
```

Note:

1. Get `<matter-node-id-of-device>` from the controller details reported by `esp-rainmaker-cli getparams <rainmaker-node-id-of-controller>`.
