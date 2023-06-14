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

## Command Response

This example also shows a demo for ESP RainMaker's command - response framework.

### Registering Commands

- Enable the command response framework using `CONFIG_ESP_RMAKER_CMD_RESP_ENABLE`
- Register your command and a handler using `esp_rmaker_cmd_register(ESP_RMAKER_CMD_CUSTOM_START, ESP_RMAKER_USER_ROLE_PRIMARY_USER | ESP_RMAKER_USER_ROLE_SECONDARY_USER, led_light_cmd_handler, false, NULL);`
    - This registers a command with id = `ESP_RMAKER_CMD_CUSTOM_START` (0x1000), which will be accessible to primary as well as secondary users (but not admin) and registers the function `led_light_cmd_handler` as the callback
- Whenever the node receives a command with id = `ESP_RMAKER_CMD_CUSTOM_START` (0x1000), it calls `led_light_cmd_handler`.
- The handler parses the data for brightness and on, calls `app_light_set_brightness()` or `app_light_set_power()` as per the data received and also sends the updated params via `esp_rmaker_param_update_and_report()`.
- **Note that the command - response framework is independent of RainMaker params. This example is updating the RainMaker params just so that the state is consistent. Going ahead, some special commands will be added for param updates so that this additional call won't be required.

### Sending Commands

The [RainMaker CLI](https://rainmaker.espressif.com/docs/cli-setup) can be used for sending commands to the node. Once the user node association (preferably from phone apps) is done and you are logged in using CLI, execute these commands:


```
$ ./rainmaker.py  create_cmd_request --timeout 60 6055F97E2008 4096 '{"brightness":50}'
Request Id: BK00t2QNe7oT12dBdh9f8X
Status: success

$ ./rainmaker.py get_cmd_requests BK00t2QNe7oT12dBdh9f8X                               
Requests: [{'node_id': '6055F97E2008', 'request_id': 'BK00t2QNe7oT12dBdh9f8X', 'request_timestamp': 1685382006, 'status': 'in_progress', 'expiration_timestamp': 1685382066}]
Total: 1

$ ./rainmaker.py get_cmd_requests BK00t2QNe7oT12dBdh9f8X
Requests: [{'node_id': '6055F97E2008', 'request_id': 'BK00t2QNe7oT12dBdh9f8X', 'request_timestamp': 1685382006, 'response_timestamp': 1685382039, 'response_data': {'status': 'success'}, 'status': 'success', 'device_status': 0, 'expiration_timestamp': 1685382066}]
```
