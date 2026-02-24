# RainMaker User API Example

This example demonstrates how to use the **RainMaker User API** component (`examples/common/rmaker_user_api`): it initializes the API with either a refresh token or username/password, then uses both the **Helper API** and the **Core API** to perform common RainMaker operations.

## What the example does

1. **Setup**: NVS, network (Wi-Fi/Ethernet via `example_connect()`), RainMaker User API init with config from menuconfig.
2. **Login callbacks**: Registers optional login success/failure callbacks.
3. **User**: Gets and prints the current user ID (`app_rmaker_user_helper_api_get_user_id`).
4. **Group**: Creates a test group (`app_rmaker_user_helper_api_create_group`), then gets all groups (`app_rmaker_user_helper_api_get_groups(NULL, ...)`) and finds the new group's `group_id`.
5. **Nodes**: Gets the nodes list and count (`app_rmaker_user_helper_api_get_nodes_list`). For each node:
   - Gets node config (`app_rmaker_user_helper_api_get_node_config`), parses device name from config.
   - Gets node connection status (`app_rmaker_user_helper_api_get_node_connection_status`).
   - Gets node params (`app_rmaker_user_helper_api_get_node_params`). If the device has a `Power` parameter, sets it (toggle) via `app_rmaker_user_helper_api_set_node_params` with a multi-node JSON payload.
   - Adds the node to the test group (`app_rmaker_user_helper_api_operate_node_to_group` with `operation: add`).
6. **Group again**: Gets the group's nodes (`app_rmaker_user_helper_api_get_groups(test_group_id, ...)`), then deletes the test group (`app_rmaker_user_helper_api_delete_group`).
7. **Core API**: Calls `app_rmaker_user_api_generic()` with a custom request config for `user/nodes` (full query params: node_details, status, config, params, show_tags, is_matter) and prints status code and response.
8. **Cleanup**: Frees buffers and calls `app_rmaker_user_api_deinit()`.

All output is sent to the serial console (ESP log).

## Prerequisites

- **ESP-IDF** (5.x or later recommended).
- **Wi-Fi or Ethernet**: The example uses `protocol_examples_common` (e.g. `example_connect()`). Ensure the example is built in an environment where `protocol_examples_common` is available (e.g. under ESP-IDF examples).
- **RainMaker account**: You need either:
  - A **refresh token**, or
  - **Username (email) and password**
  for the RainMaker cloud. How to obtain a refresh token is described in the main component [README](../../README.md) or in the [RainMaker documentation](https://rainmaker.espressif.com/).

## Configuration

Run `idf.py menuconfig`, open **Example Configuration** (and optionally **RainMaker User API Configuration**), and set:

| Option | Description |
|--------|-------------|
| **RainMaker login method** | Choose **Use refresh token** or **Use username and password**. |
| **RainMaker refresh token** | Your refresh token (only if login method is refresh token). |
| **RainMaker username** | Your RainMaker email (only if login method is username and password). |
| **RainMaker password** | Your RainMaker password (only if login method is username and password). |
| **RainMaker API base endpoint url** | Base URL for the RainMaker API (default: `https://api.rainmaker.espressif.com`). |

Enable **Support helper RainMaker User API** under **RainMaker User API Configuration** if you want to use the Helper API (this example requires it).

Configure Wi-Fi (or Ethernet) in menuconfig as usual so that `example_connect()` succeeds.

## Build and run

From the example directory (or the ESP-IDF project that contains this example):

```bash
idf.py set-target <target>
idf.py menuconfig   # set RainMaker credentials and Wi-Fi
idf.py build
idf.py -p <PORT> flash monitor
```

Replace `<target>` with your chip (e.g. `esp32`, `esp32s3`) and `<PORT>` with the serial port.

## Notes

- The example creates a group named `test_group`, adds all existing nodes to it, then deletes the group. Ensure your RainMaker account has at least one node if you want to see node-related logs.
- Any string returned by the API that the example prints or parses (e.g. `user_id`, `groups`, `node_list`, `response_data`) is allocated by the component; the example frees it with `free()`.
- For more API details, see the main component [README](../../README.md) and the [RainMaker User API reference](https://swaggerapis.rainmaker.espressif.com/).

