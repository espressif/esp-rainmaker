# Matter + Rainmaker Controller On ESP32-S3-BOX Example

## What to expect in this example?

- This demonstrates a Matter + RainMaker Controller which running on [ESP32-S3-BOX](https://github.com/espressif/esp-box). Matter is used for commissioning (also known as Wi-Fi provisioning) and local control, whereas RainMaker is used for remote control.
- Please check the board (ESP32-S3-BOX / ESP32-S3-BOX-3) by `idf.py menuconfig`, default is ESP32-S3-BOX-3.
- To commission the Controller on ESP32-S3-BOX, scan the QR Code shown on the screen using ESP RainMaker app.
- After commissioning successfully, the Controller could control the other devices (supporting On/Off cluster server) in the same fabric locally by clicking the button on the screen.
- Besides the [Matter + Rainmaker light](../matter_light/), a Matter-only light at '$ESP_MATTER_PATH/examples/light' can also be commissioned by using a similar method. The QR code for Matter-only light is in the [website](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html#commissioning-and-control).
- Long click the BOOT button (on the upper left side of the screen) will reset factory, then the device can be commissioned again.

> Please refer to the [README in the parent folder](../README.md) for instructions.
