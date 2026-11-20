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



## BLE mode

`CONFIG_APP_NETWORK_BLE_MODE` selects when the BLE session runs. It applies to Wi-Fi and Thread
builds alike, and it is available when the provisioning transport is BLE.

The session always carries the `prov-session` and `proto-ver` endpoints. Other modules add their
own endpoints to it from the `NETWORK_PROV_INIT` and `NETWORK_PROV_START` events.

The session uses the provisioning security, `CONFIG_APP_PROV_SECURITY_VERSION`, in every mode and
boot state. A client reads it as `sec_ver` from `proto-ver`. HTTP local control keeps its own
`CONFIG_ESP_RMAKER_LOCAL_CTRL_SECURITY`.


| Mode                                       | Unprovisioned boot                                                                                                             | Provisioned boot                            |
| ------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------- |
| `APP_NETWORK_BLE_MODE_PROV_ONLY` (default) | Session with the provisioning endpoints. BLE is released when provisioning completes.                                          | No session.                                 |
| `APP_NETWORK_BLE_MODE_ALWAYS`              | Session with the provisioning endpoints. The session stays up when provisioning completes or the provisioning timeout expires. | Session without the provisioning endpoints. |
| `APP_NETWORK_BLE_MODE_SESSION_ONLY`        | Session without the provisioning endpoints. The network stack is never initialised.                                            | Same as unprovisioned boot.                 |

The `ALWAYS` and `SESSION_ONLY` modes keep BLE up for the whole boot, and BLE and Wi-Fi then share
RAM. On small targets such as ESP32-C2 and ESP32-C3, reduce the footprint of the application.
The RainMaker examples have an opt-in sdkconfig fragment for this. Their READMEs show how to use it.

### One BLE client at a time

The device advertises only while no BLE client is connected. A new client can connect after the
current client disconnects, or after its link times out. A client that stays connected and sends
nothing blocks other BLE clients until then. Sessions on other transports, such as HTTP local
control, are not affected.



### Limitations of BLE session only

A node in this mode never joins a network, so `app_network_start()` returns without waiting for a
connection and no `IP_EVENT_STA_GOT_IP` or `OPENTHREAD_EVENT_ATTACHED` ever arrives. Anything in
the application that waits on the network stays blocked. The BLE session itself is unaffected.

A RainMaker application must set `local_only` in `esp_rmaker_config_t`. The examples set it to
`!app_network_provides_network()`. In this mode:

- `esp_rmaker_task` does not wait for the network. It posts `RMAKER_EVENT_STARTED` and returns.
If `local_only` is not set, the task blocks the work queue on the network-up event.
- MQTT calls return `ESP_ERR_INVALID_STATE`, so the node config and param updates are not reported.
- `esp_rmaker_ota_enable()` returns `ESP_ERR_NOT_SUPPORTED`. An image in
`ESP_OTA_IMG_PENDING_VERIFY` is marked valid when the BLE session starts.
- `esp_rmaker_wifi_reset()` is a no-op, because Wi-Fi is never initialised. Factory reset still
works.
- (RainMaker) The node has no time source. A client must write the `Timestamp` param on connect,
or schedules do not run. A power cycle loses the time. `CONFIG_ESP_RMAKER_TIME_CURRENT_TIME_PARAM`
adds the param.
- (RainMaker) Self claiming needs a network. Flash a client certificate, or use assisted claiming
(`CONFIG_ESP_RMAKER_ASSISTED_CLAIM`).



## Requirements

- ESP-IDF 5.1 or later
- ESP RainMaker framework
