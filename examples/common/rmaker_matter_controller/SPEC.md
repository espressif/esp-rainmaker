# Matter Controller Setup Service

The Matter Controller Setup Service offers an interface for managing the Matter Controller device within the RainMaker Matter fabric.

## 1. Service

| Type                                | Name               |
|-------------------------------------|--------------------|
| esp.service.matter-controller-setup | **MatterCTLSetup** |

**Note**: `esp.service.matter-controller-setup` service requires `esp.service.rmaker-user-auth` service to authorize.

## 2. Parameters

| Type                        | Name          | Value Type | Default | Flag  |
|-----------------------------|---------------|------------|---------|-------|
| esp.param.rmaker-group-id   | RMakerGroupID | string     |         | R W P |
| esp.param.matter-ctl-cmd    | MTCtlCMD      | int        | -1      | W     |
| esp.param.matter-ctl-status | MTCtlStatus   | int        | 0       | R P   |

**Note**: If `esp.param.rmaker-group-id` is available, Phone APP should not prompt the user to select a group again.

### 2.1 RMakerGroupID Parameter

This parameter stores the Rainmaker Group Id which is bound to the Matter Fabric Id. In RainMaker Matter Fabric, each RainMaker group corresponds to a Matter Fabric. If this parameter already has a non-empty value, it should not be updated in any case.

### 2.2 MTCtlCMD Parameter

This parameter corresponds to the command that the cloud sends to the Matter Controller. Here defines two command enumerations.

| Command Code Value | Command Name     |
|--------------------|------------------|
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
|-----|----------------------|-------------------------------------------------------------------------------|
| 0   | BaseURLSet           | Whether the BaseURL is received from `esp.service.rmaker-user-auth` service   |
| 1   | UserTokenSet         | Whether the UserToken is received from `esp.service.rmaker-user-auth` service |
| 2   | RmakerGroupIDSet     | Whether the RMakerGroupID parameter is set                                    |
| 3   | ControllerSetup      | Whether the controller has been successfully set up before                    |
| 4   | MatterCASEPermission | Whether the Phone APP should establish a CASE with the device                 |

**Note**: The MatterCASEPermission bit takes effect only after the ControllerSetup bit is set to true. Before that, since the controller is not yet set up, there is no restriction on the Phone APP establishing a CASE with the device.

## 3. Matter Controller Initialization

Steps for Matter controller initialization.

- Matter controller get network credentials with wifi-provisioning or other methods (e.g., Matter commissioning).

- Phone APP authorizes the device with `esp.service.rmaker-user-auth` service.

- Phone APP sends `setparams` command with the `--data` payload `{"MatterCTLSetup":{"RMakerGroupID": <rainmaker-group-id>}}`.
  * If the device was added via Matter commissioning, `<rainmaker-group-id>` must match the group selected during commissioning.
  * If the device is not added via Matter commissioning, `<rainmaker-group-id>` should be the selected group.

- Phone APP receives report of `MTCtlStatus` with 0-3 bits set to `true` before timeout.

- Phone APP sends `setparams` command with the `--data` payload `{"MatterCTLSetup":{"MTCtlCMD": 2}}` to make the controller obtain the device list in the Matter Fabric. The same command should also be sent whenever a Matter device is added or removed
