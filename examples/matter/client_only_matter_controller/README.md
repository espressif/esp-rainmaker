# Non-Server Matter Controller Example

## Build and Flash firmware

Follow the ESP RainMaker Documentation [Get Started](https://rainmaker.espressif.com/docs/get-started.html) section to build and flash this firmware. Just note the path of this example.

## What to expect in this example?

- This example uses [wifi_provision](https://github.com/espressif/esp-idf/tree/master/components/wifi_provisioning) to provision a Matter Controller into a Wi-Fi network.

- Use [RainMaker CLI](../../../cli/) to setup the Matter Controller.

- Use [device console](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html#matter-controller) to send Matter commands to other Matter End-Devices.

## Steps to use this example

- Use [RainMaker Phone App](https://github.com/espressif/esp-rainmaker/blob/master/README.md#phone-apps) to provision the Matter Controller example to RainMaker home as a basic RainMaker device.

- Login your RainMaker account to get the refres-token and access-token:

```
curl -X 'POST' \
  '<base-url>/v1/login2'\
  -H 'accept: application/json'\
  -H 'Content-Type: application/json'\
  -d '{ "user_name": "<user-name>", "password": "<password>" }'
{"status":"success","description":"Login successful","idtoken": <id-token>, "accesstoken": "<access-token>", "refreshtoken": "<refres-token>"}
```

**Note**: The default `base-url` should be "https://api.rainmaker.espressif.com" if using public server.

- Get the RainMaker Group(Matter Fabric) which you want the controller to join in:

```
curl -X 'GET' \
  '<base-url>/v1/user/node_group?node_list=false&sub_groups=false&node_details=true&is_matter=true&fabric_details=false&num_records=25' \
  -H 'accept: application/json' \
  -H 'Authorization: <access-token>'
{"groups":[{"group_id":"<rmaker-group-id>","group_name":"<group-name>","fabric_id":"<matter-fabric-id>","is_matter":true,"primary":true,"total":0}, ...],"total":<group-num>}
```

- Get the Controller's RainMaker Node ID from your phone APP or RainMaker CLI:

```
esp-rainmaker-cli getnodes
<controller-rmaker-node-id>
<other-device-rmaker-node-id>
...
```

- Send `setparams` command with RainMaker CLI:

```
esp-rainmaker-cli setparams --data '{"MatterCTL":{"BaseURL": <base-url>, "UserToken": <refresh-token>, "RMakerGroupID": <rmaker-group-id>}}' <controller-rmaker-node-id>
```


- Update the device list of the Controller's Fabric with RainMaker CLI:

```
esp-rainmaker-cli setparams --data '{"MatterCTL":{"MTCtlCMD": 2}}' <controller-rmaker-node-id>
```

- Get the node list from the controller's device console

```
> matter esp dev_mgr print

I (929673) MATTER_DEVICE: device 0 : {
I (929673) MATTER_DEVICE:     rainmaker_node_id: <other-device-rmaker-node-id>,
I (929673) MATTER_DEVICE:     matter_node_id: <other-device-matter-node-id>,
I (929683) MATTER_DEVICE:     is_rainmaker_device: false,
I (929683) MATTER_DEVICE:     is_online: false,
I (929703) MATTER_DEVICE:     endpoints : [
I (929703) MATTER_DEVICE:         {
I (929713) MATTER_DEVICE:            endpoint_id: 1,
I (929713) MATTER_DEVICE:            device_type_id: 0x10d,
I (929723) MATTER_DEVICE:            device_name: Matter Accessory,
I (929723) MATTER_DEVICE:         },
I (929723) MATTER_DEVICE:     ]
I (929733) MATTER_DEVICE: }
...
Done
```

- Send [Matter commands](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html#matter-controller) to other Matter End-Devices with the device console of Matter controller.
