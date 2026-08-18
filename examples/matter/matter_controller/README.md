# RainMaker Matter Controller Example

This example runs a (client only) Matter controller working with ESP RainMaker, including features like realtime attribute reporting and remote control.

## Flow

> [!NOTE]
> For more info please refer to [RainMaker Matter Controller Component Specification](../../common/rmaker_matter_controller/SPEC.md).

- Commission the controller by scanning the QR code with the RainMaker app.
- During commissioning, follow the steps (select a group for controller + authorization).
- After necessary credentials/info are ready, controller will perform below steps automatically:
  - Installs the user NOC (issued from RainMaker).
  - Fetches Matter devices in the same fabric (the group user selected during provisioning), and publishes device state through the `MTDevices` parameter.
- The controller can control Matter devices:
  - Locally via `matter esp controller` CLI command on the device console.
  - Through RainMaker remote command-response payloads.

## Build

ESP-IDF v5.5.3 is recommeneded.

```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## RainMaker CLI

### Fetch commissioned-device details

```bash
esp-rainmaker-cli login
esp-rainmaker-cli getparams <rainmaker-node-id-of-controller>
```

Notes:

- Use the same RainMaker account that was used to claim the controller and is signed in to the RainMaker app.
- Get `<rainmaker-node-id-of-controller>` from the RainMaker app, RainMaker dashboard, or `esp-rainmaker-cli getnodes.

Example `getparams` output:

```json
{
  "MatterCTLSetup": {
    "MTCtlCMD": -1,
    "MTCtlStatus": 15,
    "MTDevices": {
      "0e28fdd57b13a1fb": {
        "endpoints": {
          "0x1": {
            "clusters": {
              "servers": {
                "0x3": {
                  "attributes": {
                    "0x0": 0,
                    "0x1": 1
                  }
                },
                "0x300": {
                  "attributes": {
                    "0x10": 0,
                    "0x2": 0,
                    "0x3": 24939,
                    "0x4": 24701,
                    "0x4001": 2,
                    "0x400A": 24,
                    "0x400B": 1,
                    "0x400C": 65279,
                    "0x400D": 1,
                    "0x7": 250,
                    "0x8": 2,
                    "0xF": 0
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
                    "0x4002": 0
                  }
                },
                "0x62": {
                  "attributes": {
                    "0x1": 16,
                    "0x2": [
                      {
                        "0x0": 0,
                        "0x4": 7,
                        "0xFE": 1
                      },
                      {
                        "0x0": 0,
                        "0x1": 0,
                        "0x2": 0,
                        "0x3": false,
                        "0x4": 7,
                        "0xFE": 2
                      }
                    ]
                  }
                },
                "0x8": {
                  "attributes": {
                    "0x0": 64,
                    "0x1": 0,
                    "0x11": 64,
                    "0x4000": 64,
                    "0xF": 0
                  }
                }
              }
            }
          }
        },
        "online": true,
        "rainmaker_node_id": "XXXXXXXXXXXXXXXXXXXXXX"
      },
      "2de6ede976f2a382": {
        "endpoints": {
          "0x1": {
            "clusters": {
              "servers": {
                "0x3": {
                  "attributes": {
                    "0x0": 0,
                    "0x1": 2
                  }
                },
                "0x3B": {
                  "attributes": {
                    "0x0": 2,
                    "0x1": 0,
                    "0x2": 5
                  }
                }
              }
            }
          }
        },
        "online": true,
        "rainmaker_node_id": "YYYYYYYYYYYYYYYYYYYYYY"
      },
      "revision": 0
    },
    "RMakerGroupID": "ZZZZZZZZZZZZZZZZZZZZZZ"
  },
  "MatterController": {
    "Name": "MatterController"
  },
  "RMUserAuth": {
    "BaseURL": "https://api.rainmaker.espressif.com",
    "UserToken": "REDACTED",
    "UserTokenStatus": 1
  },
  "Scenes": {
    "Scenes": []
  },
  "Schedule": {
    "Schedules": []
  },
  "System": {
    "Factory-Reset": false,
    "Reboot": false,
    "Wi-Fi-Reset": false
  },
  "Time": {
    "TZ": "",
    "TZ-POSIX": ""
  }
}
```

### Remote Control

Remote control uses RainMaker command-response commands. The controller currently exposes Matter invoke, write, and read commands; see [Remote Matter Control](../../common/rmaker_matter_controller/SPEC.md#4-remote-matter-control) for the full payload schema.

Simple On/Off toggle example using Matter `OnOff` cluster command `Toggle`:

```bash
esp-rainmaker-cli create_cmd_request <rainmaker-node-id-of-controller> 4352 '{"objects":[{"matter_node_id":"0x0e28fdd57b13a1fb","matter_endpoint_id":"0x1"}],"request_payload":{"cluster_id":"0x6","command_id":"0x2","command_fields":{}}}' --timeout 60
```

The command returns a RainMaker command request ID. Query it to get the Matter command result:

```bash
esp-rainmaker-cli get_cmd_requests <request_id>
```

A successful Matter invoke response contains one entry per target object:

```json
{
  "responses": [
    {
      "matter_node_id": "0x0e28fdd57b13a1fb",
      "matter_endpoint_id": "0x1",
      "status": "success"
    }
  ]
}
```

Use `4352` (`0x1100`) for invoke, `4353` (`0x1101`) for write attribute, and `4354` (`0x1102`) for read attribute. Get `<matter-node-id>` and endpoint/cluster/attribute information from `MTDevices` in the `getparams` output above.

## OpenThread Border Router

Use `sdkconfig.defaults.otbr` to enable OTBR support (a ESP32-H2 board flashed with the `ot_rcp` example is needed):

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.otbr" set-target esp32s3 build
idf.py -p <PORT> erase-flash flash monitor
```

> [!IMPORTANT]
> User need to update thread dataset manually in the RainMaker app (`Update Thread Dataset` in the device detail menu).

Thread Border Router is initialized after the dataset is configured and has an IP address, `ot_cli` commands is also available in device console:

```bash
matter esp ot_cli <command>
```
