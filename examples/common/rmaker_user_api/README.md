# RainMaker User API

C client for the [Espressif RainMaker](https://rainmaker.espressif.com/) cloud API, built on the [RainMaker User API](https://swaggerapis.rainmaker.espressif.com/). It provides a low-level executor for any User API endpoint and an optional set of helper APIs for common operations (user, nodes, groups, node mapping, etc.).

## Overview

- **Core API** (`app_rmaker_user_api.h`): Initialize with refresh token or username/password, then call any RainMaker User API via `app_rmaker_user_api_generic()` with a request config (type, name, version, query params, payload). Handles API version resolution, login, access token, and optional persistent HTTP connection.
- **Helper API** (`app_rmaker_user_helper_api.h`): Optional wrapper layer. When `CONFIG_ENABLE_RM_USER_HELPER_API` is enabled, provides direct functions for user ID, nodes list, node config/params, set params, groups (get/create/delete/operate nodes), node mapping (add/remove, status), and node connection status. Callers get simple C types or JSON strings; no need to build query params or parse raw responses for these operations.

## Features

- **Authentication**: Refresh token or username/password. Access token is obtained automatically; 401 triggers re-login when possible.
- **API version**: Optional in config; if not set, the component fetches the supported version from the cloud.
- **Request execution**: GET, POST, PUT, DELETE with configurable URL, query params, and JSON or form payload.
- **Optional persistent connection**: Reuse a single HTTP connection for multiple requests (configurable per request).
- **Login callbacks**: Optional success/failure callbacks for login (e.g. for UI or logging).
- **Helper API** (optional): User ID, nodes list/count, node config, get/set node params, get/create/delete group, add/remove nodes to group, node mapping (add/remove, get status), node connection status.

## Requirements

- **ESP-IDF** (5.x or later recommended)
- **Wi-Fi** (or Ethernet) for connectivity
- **cJSON** (typically provided by ESP-IDF `json` component)
- **ESP TLS / HTTP client** (ESP-IDF)

## Configuration

### Component options (Kconfig)

| Option | Description |
|--------|-------------|
| `CONFIG_ENABLE_RM_USER_HELPER_API` | Enable the helper API layer (`app_rmaker_user_helper_api.h`). When disabled, only the core API and `app_rmaker_user_api_generic()` are available. |

### Example configuration

The example under `example/rmaker_user_api/` uses:

- **Login method**: Refresh token **or** username/password (choice in menuconfig).
- **Options**: `EXAMPLE_RAINMAKER_REFRESH_TOKEN`, `EXAMPLE_RAINMAKER_USERNAME`, `EXAMPLE_RAINMAKER_PASSWORD`, `EXAMPLE_RAINMAKER_BASE_URL` (default: `https://api.rainmaker.espressif.com`).

Build and run the example from the ESP-IDF examples directory or add this component as a dependency and configure the same options in your project.

## Usage

### 1. Initialize

```c
#include "app_rmaker_user_api.h"

app_rmaker_user_api_config_t config = {
    .refresh_token = "your_refresh_token",  // or use username/password
    // .username = "user@example.com",
    // .password = "password",
    .base_url = "https://api.rainmaker.espressif.com",
    .api_version = NULL,  // optional; fetched automatically if NULL
};

esp_err_t ret = app_rmaker_user_api_init(&config);
```

### 2. Optional: register login callbacks

```c
app_rmaker_user_api_register_login_failure_callback(my_login_failure_cb);
app_rmaker_user_api_register_login_success_callback(my_login_success_cb);
```

### 3. Call APIs

**Using the core API only** (any User API endpoint):

```c
app_rmaker_user_api_request_config_t req = {
    .reuse_session = true,
    .no_need_authorize = false,
    .payload_is_json = false,
    .api_type = APP_RMAKER_USER_API_TYPE_GET,
    .api_name = "user/nodes",
    .api_version = "v1",
    .api_query_params = "node_details=true&status=true&config=false&params=false",
    .api_payload = NULL,
};

int status_code = 0;
char *response_data = NULL;
ret = app_rmaker_user_api_generic(&req, &status_code, &response_data);
if (ret == ESP_OK && response_data) {
    // use response_data; caller must free(response_data)
}
free(response_data);
```

**Using the helper API** (when `CONFIG_ENABLE_RM_USER_HELPER_API` is set):

```c
#include "app_rmaker_user_helper_api.h"

char *user_id = NULL;
app_rmaker_user_helper_api_get_user_id(&user_id);

char *nodes_list = NULL;
uint16_t nodes_count = 0;
app_rmaker_user_helper_api_get_nodes_list(&nodes_list, &nodes_count);

char *groups = NULL;
app_rmaker_user_helper_api_get_groups(NULL, &groups);  // NULL = all groups

// ... use results; caller must free(...)
free(user_id);
free(nodes_list);
free(groups);
```

### 4. Deinitialize

```c
app_rmaker_user_api_deinit();
```

## API summary

### Core API (`app_rmaker_user_api.h`)

| Function | Description |
|----------|-------------|
| `app_rmaker_user_api_init()` | Initialize with config (refresh token or username/password, base URL, optional API version). |
| `app_rmaker_user_api_deinit()` | Deinitialize and release resources. |
| `app_rmaker_user_api_get_api_version()` | Get current API version string (caller must free). |
| `app_rmaker_user_api_login()` | Login (optional; other APIs can trigger login automatically). |
| `app_rmaker_user_api_register_login_failure_callback()` | Register callback for login failure. |
| `app_rmaker_user_api_register_login_success_callback()` | Register callback for login success. |
| `app_rmaker_user_api_generic()` | Execute any User API by request config; returns status code and response body (caller must free). |
| `app_rmaker_user_api_set_refresh_token()` | Set refresh token. |
| `app_rmaker_user_api_get_refresh_token()` | Get a copy of current refresh token (caller must free). |
| `app_rmaker_user_api_set_username_password()` | Set username and password. |
| `app_rmaker_user_api_set_base_url()` | Set base URL. |

### Helper API (`app_rmaker_user_helper_api.h`)  
*Available when `CONFIG_ENABLE_RM_USER_HELPER_API` is enabled.*

| Function | Description |
|----------|-------------|
| `app_rmaker_user_helper_api_get_user_id()` | Get user ID (caller must free). |
| `app_rmaker_user_helper_api_get_nodes_list()` | Get nodes list JSON and count (caller must free list). |
| `app_rmaker_user_helper_api_get_node_config()` | Get node config by node_id (caller must free). |
| `app_rmaker_user_helper_api_get_node_params()` | Get node parameters (caller must free). |
| `app_rmaker_user_helper_api_set_node_params()` | Set node parameters (single or multiple nodes via JSON payload). |
| `app_rmaker_user_helper_api_get_groups()` | Get group(s); pass NULL for all (caller must free). |
| `app_rmaker_user_helper_api_create_group()` | Create a group (JSON payload). |
| `app_rmaker_user_helper_api_delete_group()` | Delete a group by group_id. |
| `app_rmaker_user_helper_api_operate_node_to_group()` | Add or remove nodes to/from a group (JSON payload). |
| `app_rmaker_user_helper_api_set_node_mapping()` | Add or remove node mapping (secret_key not required for remove). |
| `app_rmaker_user_helper_api_get_node_mapping_status()` | Get node mapping status by request_id. |
| `app_rmaker_user_helper_api_get_node_connection_status()` | Get node connection status (online/offline). |

## Example

The project includes an example in [`example/rmaker_user_api/`](example/rmaker_user_api/). It demonstrates:

- **Init** – Initialize with refresh token or username/password; register login success/failure callbacks.
- **Helper API** – Get user ID; create group `test_group`, get all groups, find group ID; get nodes list; for each node: get config, connection status, params; set node params (toggle Power if present); add node to group; get group nodes; delete group.
- **Core API** – Raw `app_rmaker_user_api_generic()` for GET `user/nodes` with custom query params.

See the [example README](example/rmaker_user_api/README.md) for flow, configuration (menuconfig), build and flash, and the list of APIs used. Configure Wi-Fi and RainMaker credentials (refresh token or username/password) in menuconfig, then build and flash.

## API reference

For full RainMaker API details (endpoints, parameters, responses), see:

- [RainMaker User API reference](https://swaggerapis.rainmaker.espressif.com/)

## License

- This component is licensed under the **Apache License 2.0**.
- See the [LICENSE](LICENSE) file in the repository for the full text.
