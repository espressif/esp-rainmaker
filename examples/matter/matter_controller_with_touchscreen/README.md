# RainMaker Matter Controller With Touchscreen Example

This example runs the controller example with a touchscreen UI. For the shared controller flow and RainMaker CLI usage, see the [Matter controller README](../matter_controller/README.md).

## Supported Boards

All of the following criteria must be met:

- The board must appear in the [`esp_board_manager` supported boards list](https://components.espressif.com/components/espressif/esp_board_manager/versions/0.5.8/readme?language=en#supported-boards).
- The board must use an `esp32s3` chip.
- The board must have a `320x240` touchscreen display.

Supported boards:

- [ESP32-S3-BOX/BOX-3](https://github.com/espressif/esp-box)
- [M5Stack-CoreS3](https://docs.m5stack.com/en/core/CoreS3)

## Touchscreen UI

- Commission the controller by scanning the QR code shown on the screen with the ESP RainMaker app.
- After commissioning, the controller shows Matter devices in the same fabric that expose supported device types.
- Tap an on-screen device card to control an On/Off device locally.
- Use the Reset button in the `About Us` page to factory reset the controller and recommission it.

## Build And Flash

```bash
idf.py set-target esp32s3
idf.py bmgr -l
idf.py bmgr -b <board>
idf.py build
idf.py flash monitor
```

Use the board-manager board name for `<board>`, for example `esp32_s3_box_3` or `m5stack_cores3`.

## OpenThread Border Router

Thread Border Router support is available by pairing the display board with an ESP32-H2 RCP. For M5Stack-CoreS3, [Module Gateway H2](https://shop.m5stack.com/products/esp32-h2-thread-zigbee-gateway-module) can be used, or you can use the [M5Stack Thread Border Router](https://shop.m5stack.com/products/m5stack-thread-border-router?variant=47023582281985) kit.

Prepare an ESP32-H2 board flashed with the ESP-IDF `ot_rcp` example and connect it to the display board over UART:

| ESP32-H2 | BOX / BOX-3 | M5Stack-CoreS3 |
| -------- | ----------- | -------------- |
| GND      | GND         | GND            |
| 3V3      | 3V3         | 3V3            |
| RX       | GPIO41      | GPIO17         |
| TX       | GPIO38      | GPIO10         |

Build this example with OTBR defaults enabled:

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.otbr" set-target esp32s3
idf.py bmgr -l
idf.py bmgr -b <board>
idf.py build
idf.py flash monitor
```

For more info please refer to `OpenThread Border Router` part in the [Matter controller README](../matter_controller/README.md).
