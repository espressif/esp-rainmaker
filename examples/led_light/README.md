# LED Light Example

## Build and Flash firmware

Follow the ESP RainMaker Documentation [Get Started](https://rainmaker.espressif.com/docs/get-started.html) section to build and flash this firmware. Just note the path of this example.

## What to expect in this example?

- This example uses the BOOT button and RGB LED on the ESP32-S2-Saola-1/ESP32-C3-DevKitC board to demonstrate a lightbulb.
- The LED acts as a lightbulb with hue, saturation and brightness.
- Pressing the BOOT button will toggle the power state of the lightbulb. This will also reflect on the phone app.
- Toggling the button on the phone app should toggle the LED on your board, and also print messages like these on the ESP32-S2 monitor:

```
I (16073) app_main: Received value = true for Lightbulb - power
```

- You may also try changing the hue, saturation and brightness from the phone app.

### LED not working?

The ESP32-S2-Saola-1 board has the RGB LED connected to GPIO 18. However, a few earlier boards may have it on GPIO 17. Please use `CONFIG_WS2812_LED_GPIO` to set the appropriate value.

### Reset to Factory

Press and hold the BOOT button for more than 3 seconds to reset the board to factory defaults. You will have to provision the board again to use it.

