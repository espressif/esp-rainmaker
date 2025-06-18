# Matter + Rainmaker Controller With Touchscreen Example

- This example demonstrates a Matter + RainMaker Controller (Matter is used for commissioning, also known as Wi-Fi provisioning, and local control, whereas RainMaker is used for remote control), which can run on following boards:
  - [ESP32-S3-BOX/BOX-3](https://github.com/espressif/esp-box)
  - [ESP32-S3-LCD-EV-BOARD](https://github.com/espressif/esp-dev-kits).
  - [M5Stack-CoreS3](https://docs.m5stack.com/en/core/CoreS3)
- Thread Border Router support by integrating an ESP32-H2. [Module Gateway H2](https://shop.m5stack.com/products/esp32-h2-thread-zigbee-gateway-module) can be used for M5Stack-CoreS3.


## Usage

### Setting [prerequisites](../README.md#prerequisites) and [environment](../README.md#setting-up-the-environment)

### [Claiming device certificates](../README.md#claiming-device-certificates)

### [Generating the factory nvs binary](../README.md#generating-the-factory-nvs-binary)

### Building the example (without OpenThread Border Router)

```
$ idf.py set-target esp32s3
$ idf.py build
$ idf.py flash monitor
```

- The `set-target` command above depends on the selected board:
  - For ESP32-S3-BOX series, set-target command should be `idf.py set-target esp32s3`.
  - For ESP32-S3-LCD-EV-BOARD, set-target command should be `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.lcdboard" set-target esp32s3`.
  - For M5Stack CoreS3, set-target command should be `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.m5stack" set-target esp32s3`.

- Please check the board selection (default ESP32-S3-BOX-3) in one of the ways:
  - Use `idf.py menuconfig`->`HMI Board Config`->`Selece BSP board` to select board.
  - Add only one board config in [sdkconfig.defaults](./sdkconfig.defaults). e.g. 'CONFIG_BSP_BOARD_ESP32_S3_LCD_EV_BOARD=y' to select ESP32-S3-LCD-EV-BOARD as board. 'CONFIG_BSP_BOARD_ESP32_S3_BOX_3=y' to select ESP32-S3-BOX-3 as board. 'CONFIG_BSP_BOARD_M5STACK_CORES3=y' to select M5Stack-CoreS3 as board.

### Building the example (with OpenThread Border Router)

Prepare a ESP32-H2 board (flashed ot_rcp example) to connect ESP32-S3-BOX / ESP32-S3-BOX-3 / S3-LCD-EV-BOARD / M5Stack-CoreS3 via UART.

| ESP32-H2 | BOX / BOX-3 | S3-LCD-EV-BOARD | M5Stack-CoreS3 |
|----------|-------------|-----------------|----------------|
|    GND   |     GND     |       GND       |      GND       |
|    3V3   |     3V3     |       3V3       |      3V3       |
|    RX    |    GPIO41   |      GPIO0      |     GPIO17     |
|    TX    |    GPIO38   |      GPIO4      |     GPIO10     |

The example with the OTBR feature supports flashing the RCP image from the host SoC.

First build the [ot_rcp](https://github.com/espressif/esp-idf/tree/master/examples/openthread/ot_rcp).

Build the example with the sdkconfig file 'sdkconfig.defaults.otbr' to enable the OTBR feature on the controller.

```
$ idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.otbr" set-target esp32s3
$ idf.py build
$ idf.py flash monitor
```

- The `set-target` command above depends on the selected board:
  - For ESP32-S3-BOX series, set-target command should be `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.otbr" set-target esp32s3`.
  - For ESP32-S3-LCD-EV-BOARD, set-target command should be `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.lcdboard;sdkconfig.defaults.otbr" set-target esp32s3`.
  - For M5Stack CoreS3, set-target command should be `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.m5stack;sdkconfig.defaults.otbr" set-target esp32s3`.

- Please check the board selection (same as with no OTBR feature)

If you are using IDF v5.3.1 or later, there might be an error of `#error CONFIG_LWIP_IPV6_NUM_ADDRESSES should be set to 12` when building this example. Please change the IPv6 addresses number for LwIP network interface in menuconfig and rebuild the example again.

> Note: Thread Border Router is not initialized until the Controller has been commissioned and obtained an IP address.

### Commissioning

To commission the Controller with touchscreen, scan the QR Code shown on the screen using ESP RainMaker app.
- After commissioning successfully, the Controller could control the other devices (supporting On/Off cluster server) in the same fabric locally by clicking the icon on the screen.
- Besides the [Matter + Rainmaker light](../matter_light/), a Matter-only light at '$ESP_MATTER_PATH/examples/light' can also be commissioned by using a similar method. The QR code for Matter-only light is on the [website](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html#commissioning-and-control).
- Long click the BOOT button or touch the Reset button in "About Us" page will do factory reset, then the device can be commissioned again.

> Please refer to the [README in the parent folder](../README.md) for instructions.


## Fetching the details of the devices commissioned in the RainMaker Fabric:

```
$ cd $RMAKER_PATH/cli
$ ./rainmaker.py login --email <rainmaker-account-email>
$ ./rainmaker.py getparams <rainmaker-node-id-of-controller>
```
**Note**:
1) Please use the same account credentials that are used to claim the controller and signed into the Rainmaker app.
2) Get the rainmaker-node-id-of-controller by logging into [ESP Rainmaker Dashboard](https://dashboard.rainmaker.espressif.com/login).


## Controlling the devices remotely using rainmaker cli:

```
$ cd $RMAKER_PATH/cli
$ ./rainmaker.py login --email <rainmaker-account-email>
```
_Toggle command:_
```
$ ./rainmaker.py setparams --data '{"Matter-Controller":{"Matter-Devices":{"matter-nodes":[{"matter-node-id":"<matter-node-id-of-device>","endpoints":[{"endpoint-id":"0x1","clusters":[{"cluster-id":"0x6","commands":[{"command-id":"0x2"}]}]}]}]}}}' <rainmaker-node-id-of-controller>
```
_Set Brightness command:_
```
$ ./rainmaker.py setparams --data '{"Matter-Controller":{"Matter-Devices":{"matter-nodes":[{"matter-node-id":"<matter-node-id-of-device>","endpoints":[{"endpoint-id":"0x1","clusters":[{"cluster-id":"0x8","commands":[{"command-id":"0x0","data":{"0:U8": 10, "1:U16": 0, "2:U8": 0, "3:U8": 0}}]}]}]}]}}}' <rainmaker-node-id-of-controller>
```
**Note**:
1) Get the matter-node-id-of-device from the details reported by the controller using getparams command.

**Node config of Matter-Controller:**
```
        {
            "name": "Matter-Controller",
            "params": [
                {
                    "data_type": "object",
                    "name": "Matter-Devices",
                    "properties": [
                        "read"
                    ],
                    "type": "esp.param.matter-devices"
                },
                {
                    "data_type": "string",
                    "name": "Matter-Controller-Data-Version",
                    "properties": [
                        "read"
                    ],
                    "type": "esp.param.matter-controller-data-version"
                }
            ],
            "type": "esp.service.matter-controller"
        }


```

**Example of getparams:**

```
{
    "Matter Controller":{
        "Name":"Controller",
        "Power":true
    },
    "Matter-Controller":{
        "Matter-Controller-Data-Version":"1.0.1",
        "Matter-Devices":{
            "d81b1178e484c26d":{
                "enabled":true,
                "endpoints":{
                    "0x1":{
                        "clusters":{
                            "servers":{
                                "0x3":{
                                    "attributes":{
                                        "0x0":0,
                                        "0x1":0
                                    }
                                },
                                "0x300":{
                                    "attributes":{
                                        "0x10":0,
                                        "0x2":0,
                                        "0x3":24939,
                                        "0x4":24701,
                                        "0x4001":2,
                                        "0x400a":24,
                                        "0x400b":0,
                                        "0x400c":65279,
                                        "0x400d":0,
                                        "0x4010":"Unhandled",
                                        "0x7":250,
                                        "0x8":2,
                                        "0xf":0
                                    }
                                },
                                "0x4":{
                                    "attributes":{
                                        "0x0":128
                                    }
                                },
                                "0x6":{
                                    "attributes":{
                                        "0x0":true,
                                        "0x4000":true,
                                        "0x4001":0,
                                        "0x4002":0,
                                        "0x4003":"Unhandled"
                                    }
                                },
                                "0x62":{
                                    "attributes":{
                                        "0x1":16,
                                        "0x2":"Unhandled"
                                    }
                                },
                                "0x8":{
                                    "attributes":{
                                        "0x0":64,
                                        "0x1":0,
                                        "0x11":"Unhandled",
                                        "0x2":1,
                                        "0x3":254,
                                        "0x4000":64,
                                        "0xf":0
                                    }
                                }
                            }
                        },
                        "device_type":"0x10d"
                    },
                    "0x2":{
                        "clusters":{
                            "servers":{
                                "0x3":{
                                    "attributes":{
                                        "0x0":0,
                                        "0x1":0
                                    }
                                },
                                "0x3b":{
                                    "attributes":{
                                        "0x0":2,
                                        "0x1":0
                                    },
                                    "events":{
                                        "0x0":false,
                                        "0x1":false,
                                        "0x2":false,
                                        "0x3":false,
                                        "0x4":false,
                                        "0x5":false,
                                        "0x6":false
                                    }
                                }
                            }
                        },
                        "device_type":"0xf"
                    },
                    "0x3":{
                        "clusters":{
                            "clients":[
                                "0x3",
                                "0x6",
                                "0x4"
                            ],
                            "servers":{
                                "0x1e":{
                                    "attributes":{
                                        "0x0":"Unhandled"
                                    }
                                },
                                "0x3":{
                                    "attributes":{
                                        "0x0":0,
                                        "0x1":0
                                    }
                                },
                                "0x4":{
                                    "attributes":{
                                        "0x0":128
                                    }
                                }
                            }
                        },
                        "device_type":"0x103"
                    }
                },
                "reachable":true
            }
        }
    },
    "Scenes":{
        "Scenes":[

        ]
    },
    "Schedule":{
        "Schedules":[

        ]
    },
    "Time":{
        "TZ":"",
        "TZ-POSIX":""
    }
}
```