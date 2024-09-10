# Thread Border Router Example

## Hardware Required

Please use the ESP Thread Border Router Board for this example. It provides an integrated module of an ESP32-S3 SoC and an ESP32-H2 RCP.

![br_dev_kit](./image/esp-thread-border-router-board.png)

## Build and Flash firmware

### Build the RCP firmware

The Border Router supports updating the RCP upon boot.

First build the [ot_rcp](https://github.com/espressif/esp-idf/tree/master/examples/openthread/ot_rcp) example in IDF.

### Build and Flash Thread Border Router Firmware

Follow the ESP RainMaker Documentation [Get Started](https://rainmaker.espressif.com/docs/get-started.html) section to build and flash this firmware. Just note the path of this example.

If you are using IDF v5.3.1 or later, there might be an error of `#error CONFIG_LWIP_IPV6_NUM_ADDRESSES should be set to 12` when building this example. Please change the IPv6 addresses number for LwIP network interface in menuconfig and rebuild the example again.

In the building process of Thread Border Router firmware, the built RCP image in IDF path will be automatically packed into the Border Router firmware.

## What to expect in this example?

- This example demonstrates an OpenThread Border Router on [ESP Thread Border Router Board](https://github.com/espressif/esp-thread-br/tree/main?tab=readme-ov-file#esp-thread-border-router-board).
- The ESP32-H2 on ESP Thread Border Router Board acts as an [OpenThread Radio-Co Processor](https://openthread.io/platforms/co-processor) and the ESP32-S3 is the host processor. They are connected by UART.
- You could set the Thread active dataset and start Thread network of the Thread Border Router with the [RainMaker CLI](https://rainmaker.espressif.com/docs/cli-setup).

### Start Thread network

After provisioning with the RainMaker phone APP, the [RainMaker CLI](https://rainmaker.espressif.com/docs/cli-setup) can be used for setting the Thread active dataset and start Thread network.

Generate a random Thread dataset, set it as active dataset and start Thread network:

```
$ ./rainmaker.py setparams --data '{"TBRService":{"ThreadCmd": 1}}' 3485187E7F68
Node state updated successfully.
```

Or set a specific active dataset and start Thread network:

```
$ ./rainmaker.py setparams --data '{"TBRService":{"ActiveDataset": "0E080000000000010000000300001235060004001FFFE00208DE45772E58CAC8CE0708FD01321F6B80688105101CBF6F4E68CBC611B52ED9A39EFD80A9030F4F70656E5468726561642D616534370102AE470410AB7CDEB095B2C453E6CE7E7DB2BC52980C0402A0F7F8"}}' 3485187E7F68
Node state updated successfully.
```
