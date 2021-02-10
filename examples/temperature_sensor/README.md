# Temperature Sensor Example

## Build and Flash firmware

Follow the ESP RainMaker Documentation [Get Started](https://rainmaker.espressif.com/docs/get-started.html) section to build and flash this firmware. Just note the path of this example.

## What to expect in this example?

- This example uses esp timer and the RGB LED on the ESP32-S2-Saola-1/ESP32-C3-DevKitC board to demonstrate a temperature sensor.
- The temperature value is changed by 0.5 every minute.
- It starts at some default value (25.0) and goes on increasing till 99.5. Then it starts reducing till it comes to 0.5. The cycle keeps repeating.
- The LED color indicates the temperature.
- LED hue changes from 200 (bluish) to 0 (reddish) as the temperature increases from 0.5 to 99.5.
- You can check the temperature changes in the phone app.

### LED not working?

The ESP32-S2-Saola-1 board has the RGB LED connected to GPIO 18. However, a few earlier boards may have it on GPIO 17. Please use `CONFIG_WS2812_LED_GPIO` to set the appropriate value.

### Reset to Factory

Press and hold the BOOT button for more than 3 seconds to reset the board to factory defaults. You will have to provision the board again to use it.

