# GPIO Example

## Build and Flash firmware

Follow the ESP RainMaker Documentation [Get Started](https://rainmaker.espressif.com/docs/get-started.html) section to build and flash this firmware. Just note the path of this example.

### BLE local control on every boot

By default, BLE runs only during provisioning. For BLE local control on every boot, build with the
opt-in `sdkconfig.defaults.ble_session` fragment:

```
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.ble_session" set-target esp32c3
idf.py menuconfig
idf.py build
```

In menuconfig, open `ESP RainMaker App Wi-Fi Provisioning` > `BLE mode`. Select
`Provisioning and always-on BLE session` or `BLE session only, no provisioning`. The fragment does
not select the mode. Read the [rmaker_app_network README](../common/rmaker_app_network/README.md#ble-mode)
before you select the session only mode.

The fragment does these:

- Adds the local control endpoints to the BLE session (`CONFIG_ESP_RMAKER_ENABLE_PROV_LOCAL_CTRL`).
- Adds the `Timestamp` param to the time service, so a client can set the clock
  (`CONFIG_ESP_RMAKER_TIME_CURRENT_TIME_PARAM`).
- On ESP32-C2, raises the NimBLE host task stack to the IDF default of 4096 bytes.
- On ESP32-C3, applies footprint trims so that BLE and Wi-Fi fit in RAM together. The trims turn
  off WPA3 and reduce some task stacks.

## What to expect in this example?

- This example just provides 3 boolean parameters, linked to 3 GPIOS.
- Toggling the buttons on the phone app should toggle the GPIOs on your board (and the LEDs, if any, connected to the GPIOs), and also print messages like these on the ESP32-S2 monitor:

```
I (16073) app_main: Received value = true for GPIO-Device - Red
```

### Reset to Factory

Press and hold the BOOT button for more than 3 seconds to reset the board to factory defaults. You will have to provision the board again to use it.
