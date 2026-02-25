# ESP RainMaker Controller

This example turns an ESP device into an **ESP RainMaker controller** node. The device runs the RainMaker agent and exposes a **UART console** with commands to manage your RainMaker nodes (list nodes, get/set parameters, schedules, config, and remove nodes) using the RainMaker User API.

## Features

- **Controller node**: Registers as an ESP RainMaker controller with the cloud.
- **CLI over UART**: Interactive REPL with commands to query and control other nodes.
- **User API integration**: Uses the RainMaker User API (with `CONFIG_ENABLE_RM_USER_HELPER_API`) for node listing, params, config, schedules, and node removal.
- **Standard RainMaker features**: Wi-Fi/Thread provisioning, OTA, timezone, scheduling, scenes, system service, and optional ESP Insights.
- **RainMaker Auth Service**: Enabled for user authentication; the CLI uses the refresh token to call the User API.
- **Controller callback**: Can receive data from child nodes (`controller_cb`) for extending local automation logic.

## Hardware

- An ESP32 series board (e.g. ESP32, ESP32-C3, ESP32-C5, ESP32-C6).
- A **Boot** button for Wi-Fi reset and factory reset (GPIO configurable in menuconfig).
- USB-UART connection for console and flashing.

## Configuration

Run `idf.py menuconfig` and open **Example Configuration**:

| Option | Description |
|--------|-------------|
| **Boot Button GPIO** | GPIO number for the Boot button (used for reset). |
| **Proof of Possession Type** | `0` = MAC, `1` = Random, `2` = None. |
| **Example README URL** | Optional URL to show in node config (e.g. this README). |

Other RainMaker and transport options (Wi-Fi/Thread, BLE provisioning, etc.) are in their respective menus.

## Build and Flash

```bash
idf.py set-target <target>   # e.g. esp32, esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

After flashing, connect to the device over UART. The console prompt is:

```text
rainmaker_controller_cli>
```

Use the phone app to provision the controller (same as other RainMaker nodes). Once provisioned and connected, the CLI commands are available.

## CLI Commands

All commands are available from the UART console. The first User API command will initialize the RainMaker API using the controller's refresh token and base URL.

| Command | Description |
|---------|-------------|
| `getnodes` | List all node IDs associated with the user. |
| `getnodedetails` | Get detailed info for all nodes (config, status, params). |
| `getparams <node_id>` | Get parameters of the given node. |
| `setparams <node_id> <json_payload>` | Set parameters (use single quotes for JSON). |
| `getnodeconfig <node_id>` | Get node configuration. |
| `getnodestatus <node_id>` | Get online/offline status of the node. |
| `getschedules <node_id>` | Get schedule information for the node. |
| `setschedule <node_id> <json_payload>` | Set schedule for the node. |
| `removenode <node_id>` | Remove user–node mapping. |
| `getheapstatus` | Print free and minimum free heap size. |
| `help` | List registered commands. |

**Example**

```text
rainmaker_controller_cli> getnodes
1. <node_id_1>
2. <node_id_2>

rainmaker_controller_cli> getnodestatus <node_id_1>
Node <node_id_1> is online
```

## Notes

- **User API**: Depends on `CONFIG_ENABLE_RM_USER_HELPER_API=y` (set in `sdkconfig.defaults` for this example).
- **Partition table**: The example uses a custom 4 MB partition table; ensure `rainmaker.py claim` uses the correct factory partition address if you use it.
- **Reset**: Long-press Boot button for Wi-Fi reset (3 s) or factory reset (10 s), as configured in the app driver.

## Related Documentation

- [ESP RainMaker](https://rainmaker.espressif.com/)
- [Time service](https://rainmaker.espressif.com/docs/time-service.html)
