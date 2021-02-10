# HomeKit Switch Example

This example demonstrates ESP RainMaker + HomeKit integration using the [esp-homekit-sdk](https://github.com/espressif/esp-homekit-sdk).

## Setup

Compiling this example would need the [esp-homekit-sdk](https://github.com/espressif/esp-homekit-sdk) repository. Clone it at a suitable location using:

```
git clone --recursive https://github.com/espressif/esp-homekit-sdk.git
```

Export the path to this repository using:

```
export HOMEKIT_PATH=/path/to/esp-homekit-sdk/
```

## Build and Flash firmware

Follow the ESP RainMaker Documentation [Get Started](https://rainmaker.espressif.com/docs/get-started.html) section to build and flash this firmware. Just note the path of this example. Note that, on bootup, the serial terminal will show 2 QR codes, one small and the other large. The smaller QR code is to be used for HomeKit pairing from the iOS Home app, whereas the larger one is supposed to be used with ESP RainMaker app.

> Note: HomeKit pairing can be done only after the device connects to the Wi-Fi network. However, if you have the MFi variant of the ESP HomeKit SDK, the QR code can be used for WAC Provisioning as well.
> The same QR code may be shown multiple times, so that it is available on screen whenever the device is in HomeKit pairing mode.

## What to expect in this example?

- This example uses the BOOT button and RGB LED on the ESP32-S2-Saola-1/ESP32-C3-DevKitC board to demonstrate a switch with HomeKit integration.
- The LED state (green color) indicates the state of the switch.
- Pressing the BOOT button will toggle the state of the switch and hence the LED. This will also reflect on the phone app.
- Toggling the button on the phone app should toggle the LED on your board, and also print messages like these on the ESP32-S2 monitor:

```
I (16073) app_main: Received value = true for Switch - power
```
- Once the board is set up, it can also be paired from iOS Home App. Follow the steps as given in next section.

## Using with iOS Home app
Open the Home app on your iPhone/iPad and follow these steps:

- Tap on "Add Accessory" and scan the small QR code mentioned above.
- If QR code is not visible correctly, you may use the link printed on the serial terminal or follow these steps:
    - Choose the "I Don't Have a Code or Cannot Scan" option.
    - Tap on "ESP RainMaker Device" in the list of Nearby Accessories.
    - Select the "Add Anyway" option for the "Uncertified Accessory" prompt.
    - Enter 11122333 as the Setup code.
- You should eventually see the "ESP RainMaker Device added" message.
- Give a custom name, assign to a room, create scenes as required and you are done.

Now, any changes from ESP RainMaker will reflect on HomeKit and vice-versa. Changes from push button will reflect on both.

### LED not working?

The ESP32-S2-Saola-1 board has the RGB LED connected to GPIO 18. However, a few earlier boards may have it on GPIO 17. Please use `CONFIG_WS2812_LED_GPIO` to set the appropriate value.

### Reset to Factory

Press and hold the BOOT button for more than 3 seconds to reset the board to factory defaults. You will have to provision the board again to use it.
