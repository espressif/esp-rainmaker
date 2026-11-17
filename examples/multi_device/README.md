# Multi Device Example

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

- This example just demonstrates how you can have multiple devices on the same board.
- It has 4 devices
    - Switch
    - Lightbulb
    - Fan
    - Temperature Sensor
- It uses the BOOT button and RGB LED on the ESP32-S2-Saola-1/ESP32-C3-DevKitC board to demonstrate a switch, and the esp timer to demonstrate the temperature sensor.
- The LED state (green color) indicates the state of the switch.
- Pressing the BOOT button will toggle the state of the switch and hence the LED. This will also reflect on the phone app.
- Toggling the button on the phone app should toggle the LED on your board, and also print messages like these on the ESP32-S2 monitor:

```
I (16073) app_main: Received value = true for Switch - power
```

- The temperature value is changed by 0.5 every minute.
- It starts at some default value (25.0) and goes on increasing till 99.5. Then it starts reducing till it comes to 0.5. The cycle keeps repeating.
- You can check the temperature changes in the phone app.
- Lightbulb and Fan are dummy devices, but you can try setting the values from the phone app and see them reflect on the ESP32-S2 monitor.

### LED not working?

The ESP32-S2-Saola-1 board has the RGB LED connected to GPIO 18. However, a few earlier boards may have it on GPIO 17. Please use `CONFIG_WS2812_LED_GPIO` to set the appropriate value.

### Reset to Factory

Press and hold the BOOT button for more than 3 seconds to reset the board to factory defaults. You will have to provision the board again to use it.

