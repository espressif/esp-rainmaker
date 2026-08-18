# Matter Controller Setup Service

The Matter Controller Setup Service offers an interface for managing the Matter Controller device within the RainMaker Matter fabric.

## 1. Service

| Type                                | Name               |
| ----------------------------------- | ------------------ |
| esp.service.matter-controller-setup | **MatterCTLSetup** |

**Note**: `esp.service.matter-controller-setup` service requires `esp.service.rmaker-user-auth` service to authorize.

## 2. Parameters

| Type                        | Name          | Value Type | Default | Flag  |
| --------------------------- | ------------- | ---------- | ------- | ----- |
| esp.param.rmaker-group-id   | RMakerGroupID | string     |         | R W P |
| esp.param.matter-ctl-cmd    | MTCtlCMD      | int        | -1      | W     |
| esp.param.matter-ctl-status | MTCtlStatus   | int        | 0       | R P   |
| esp.param.matter-devices    | MTDevices     | object     | {}      | R     |

**Note**: If `esp.param.rmaker-group-id` is available, Phone APP should not prompt the user to select a group again.

### 2.1 RMakerGroupID Parameter

This parameter stores the Rainmaker Group Id which is bound to the Matter Fabric Id. In RainMaker Matter Fabric, each RainMaker group corresponds to a Matter Fabric. If this parameter already has a non-empty value, it should not be updated in any case.

### 2.2 MTCtlCMD Parameter

This parameter corresponds to the command that the cloud sends to the Matter Controller. Here defines two command enumerations.

| Command Code Value | Command Name     |
| ------------------ | ---------------- |
| 1                  | UpdateNOC        |
| 2                  | UpdateDeviceList |

#### 2.2.1 UpdateNOC command

The UpdateNOC command allows the controller to fetch the Rainmaker Controller NOC. When receiving this command, the controller will generate a new CSR and send it to the cloud. After receiving the response, it will install the new NOC.

#### 2.2.2 UpdateDeviceList command

This UpdateDeviceList command allows the controller to fetch the Matter devices in its RainMaker Group(Matter Fabric).

**Note**: This command SHALL be executed when a Matter device is added/removed.

### 2.3 MTCtlStatus Parameter

This parameter is a bitmap value which corresponds to the status of Matter Controller

| Bit | Name                 | Summary                                                                       |
| --- | -------------------- | ----------------------------------------------------------------------------- |
| 0   | BaseURLSet           | Whether the BaseURL is received from `esp.service.rmaker-user-auth` service   |
| 1   | UserTokenSet         | Whether the UserToken is received from `esp.service.rmaker-user-auth` service |
| 2   | RmakerGroupIDSet     | Whether the RMakerGroupID parameter is set                                    |
| 3   | ControllerSetup      | Whether the controller has been successfully set up before                    |
| 4   | MatterCASEPermission | Whether the Phone APP should establish a CASE with the device                 |

**Note**: The MatterCASEPermission bit takes effect only after the ControllerSetup bit is set to true. Before that, since the controller is not yet set up, there is no restriction on the Phone APP establishing a CASE with the device.

### 2.4 MTDevices Parameter

This parameter belongs to the Matter Controller Setup Service. It reports Matter end-device online state and latest attribute state. The controller intentionally reports `{}` during RainMaker startup to reset stale backend state, then publishes online and attribute deltas from Matter subscription reports.

Matter reporting is initialized lazily by `app_rmaker_matter_report_enable()` when a successful device-list update is processed.

#### 2.4.1 Device List And Subscription Ownership

`app_rmaker_matter_device_list_update()` fetches a temporary Matter device list from RainMaker. On success, the list is passed to `app_rmaker_matter_report_on_device_list_update()` and then to the application callback. The list is valid only during those calls.

Applications that need to retain the list must copy it with `app_rmaker_device_list_copy_create()` and later free it with `app_rmaker_device_list_copy_destroy()`.

Each tracked node retains its latest accepted attribute snapshot across subscription loss and retry. The first subscription after controller startup still uploads all accepted values because startup creates an empty snapshot and intentionally resets the RainMaker `MTDevices` parameter. Recovery subscriptions upload only values that differ from the retained snapshot; online and offline updates remain independent of attribute-value filtering.

#### 2.4.2 Reported JSON Shape

`MTDevices` is keyed by Matter node ID. Each node entry contains online state, RainMaker node ID, and endpoint/cluster/attribute state.

```json
{
  "MatterCTLSetup": {
    "MTDevices": {
      "676faf22d3151705": {
        "online": true,
        "rainmaker_node_id": "3JphZTNLqr3MxpSrkn8dvj",
        "endpoints": {
          "0x1": {
            "clusters": {
              "servers": {
                "0x6": {
                  "attributes": {
                    "0x0": true
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
```

For nodes with dynamic endpoints, the controller uses endpoint 0's Descriptor `PartsList` as the authoritative endpoint set. A newly listed endpoint is retained for topology filtering but is not reported until its first attribute arrives. A removed endpoint is reported as `null`:

```json
{
  "676faf22d3151705": {
    "rainmaker_node_id": "3JphZTNLqr3MxpSrkn8dvj",
    "endpoints": {
      "0x13": null
    }
  }
}
```

`PartsList` is consumed internally for topology tracking and is not included in `MTDevices` or the local attribute callback. Subscription loss retains the last endpoint keys but marks their topology stale, allowing priming attributes for newly added endpoints. The next valid root `PartsList` reconciles those retained keys, including reporting `null` for endpoints removed while the subscription was unavailable, and restores authoritative endpoint membership filtering.

#### 2.4.3 Local Attribute Callback

Applications can register a local callback with `app_rmaker_matter_report_set_callback()`.

| Report Type | Data Shape |
| ----------- | ---------- |
| Online      | `{"online": true|false}` |
| Attribute   | `{"<node_id>": {"<endpoint_id>": {"<cluster_id>": {"<attribute_id>": {"value": ...}}}}}` |

The callback receives data owned by the report task and valid only for the duration of the callback. Copy `report->data` if it must be retained.

#### 2.4.4 Batching And Rate Limiting

Matter subscription callbacks serialize TLV values into owned queue items before returning. The worker task compares each accepted value with the cached JSON state, retains changes in a pending RainMaker delta, and then advances the cache. Object comparison is key-based and case-sensitive; array order remains significant. Final payload canonicalization and hash-based duplicate suppression are not used.

RainMaker publishes are rate-limited by a token bucket:

| Config                                           | Summary                                                |
| ------------------------------------------------ | ------------------------------------------------------ |
| `CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_CAPACITY` | Number of quick publishes allowed after an idle period |
| `CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_REFILL_MS` | Sustained token refill interval in milliseconds        |

Nearby attribute changes are coalesced internally before publishing. The coalesce delay is intentionally not exposed as Kconfig.

## 3. Matter Controller Initialization

Steps for Matter controller initialization.

- Matter controller get network credentials with wifi-provisioning or other methods (e.g., Matter commissioning).

- Phone APP authorizes the device with `esp.service.rmaker-user-auth` service.

- Phone APP sends `setparams` command with the `--data` payload `{"MatterCTLSetup":{"RMakerGroupID": <rainmaker-group-id>}}`.
  - If the device was added via Matter commissioning, `<rainmaker-group-id>` must match the group selected during commissioning.
  - If the device is not added via Matter commissioning, `<rainmaker-group-id>` should be the selected group.

- Phone APP receives report of `MTCtlStatus` with 0-3 bits set to `true` before timeout.

- Phone APP sends `setparams` command with the `--data` payload `{"MatterCTLSetup":{"MTCtlCMD": 2}}` to make the controller obtain the device list in the Matter Fabric. The same command should also be sent whenever a Matter device is added or removed

## 4. Remote Matter Control

Remote Matter control uses RainMaker command-response commands. The controller registers three commands for Matter invoke, write, and read operations when `app_rmaker_matter_cmd_resp_enable()` is called by `app_rmaker_matter_controller_enable()`.

| Command ID    | Command Name   | Summary                               |
| ------------- | -------------- | ------------------------------------- |
| 4352 (0x1100) | InvokeCommand  | Invoke one Matter cluster command     |
| 4353 (0x1101) | WriteAttribute | Write one Matter attribute            |
| 4354 (0x1102) | ReadAttribute  | Read Matter attributes/events by path |

### 4.1 InvokeCommand

The request payload contains the target Matter node/endpoint and Matter command path.

```json
{
  "objects": [
    {
      "matter_node_id": "0x676FAF22D3151705",
      "matter_endpoint_id": "0x1"
    }
  ],
  "request_payload": {
    "cluster_id": "0x6",
    "command_id": "0x02",
    "command_fields": {}
  }
}
```

Example:

```shell
esp-rainmaker-cli create_cmd_request <node_id> 4352 '<payload>' --timeout 60
esp-rainmaker-cli get_cmd_requests <request_id>
```

A successful response contains one response entry per requested object.

```json
{
  "responses": [
    {
      "matter_node_id": "0x676FAF22D3151705",
      "matter_endpoint_id": "0x1",
      "status": "success"
    }
  ]
}
```

### 4.2 WriteAttribute

The request payload contains the target Matter node/endpoint, Matter attribute path, and Matter TLV-JSON attribute value.

```json
{
  "objects": [
    {
      "matter_node_id": "0x676FAF22D3151705",
      "matter_endpoint_id": "0x1"
    }
  ],
  "request_payload": {
    "cluster_id": "0x06",
    "attribute_id": "0x4001",
    "attribute_value": {
      "0:U16": 1
    }
  }
}
```

Example:

```shell
esp-rainmaker-cli create_cmd_request <node_id> 4353 '<payload>' --timeout 60
esp-rainmaker-cli get_cmd_requests <request_id>
```

### 4.3 ReadAttribute

The request payload contains a Matter node ID and the attribute paths to read. Wildcard values are allowed where the Matter controller command supports them.

```json
{
  "matter_node_id": "0x676FAF22D3151705",
  "attribute_paths": [
    {
      "endpoint_id": "0xFFFF",
      "cluster_id": "0xFFFFFFFF",
      "attribute_id": "0xFFF9"
    }
  ]
}
```

Example:

```shell
esp-rainmaker-cli create_cmd_request <node_id> 4354 '<payload>' --timeout 60
esp-rainmaker-cli get_cmd_requests <request_id>
```

The response contains `read_results` with Matter path fields and the decoded JSON attribute value.
