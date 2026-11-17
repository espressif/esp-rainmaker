# Fan Example

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
- On ESP32-C2, raises the NimBLE host task stack to the IDF default of 4096 bytes.
- On ESP32-C3, applies footprint trims so that BLE and Wi-Fi fit in RAM together. The trims turn
  off WPA3 and reduce some task stacks.

## What to expect in this example?

- This example uses the BOOT button and RGB LED on the ESP32-S2-Saola-1/ESP32-C3-DevKitC board to demonstrate a fan.
- The LED brightness indicates the speed of the Fan.
- Pressing the BOOT button will increase the speed by 1. The speed will cycle from 0 to 5 and then come back to 0.
- Toggling the button on the phone app should toggle the LED on your board, and also print messages like these on the ESP32-S2 monitor:

```
I (16073) app_main: Received value = true for Fan - power
```

- You may also try changing the speed from the phone app.

### LED not working?

The ESP32-S2-Saola-1 board has the RGB LED connected to GPIO 18. However, a few earlier boards may have it on GPIO 17. Please use `CONFIG_WS2812_LED_GPIO` to set the appropriate value.

### Reset to Factory

Press and hold the BOOT button for more than 3 seconds to reset the board to factory defaults. You will have to provision the board again to use it.

