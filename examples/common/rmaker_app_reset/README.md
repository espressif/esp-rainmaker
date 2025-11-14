# RainMaker App Reset Component

[![Component Registry](https://components.espressif.com/components/espressif/rmaker_app_reset/badge.svg)](https://components.espressif.com/components/espressif/rmaker_app_reset)

A simple helper component for ESP RainMaker applications that provides Wi-Fi reset and factory reset functionality using button long-press detection.

## Features

- **Wi-Fi Reset**: Reset Wi-Fi credentials without clearing other NVS data
- **Factory Reset**: Complete factory reset (clears entire NVS partition)
- **Button Integration**: Uses the upstream [espressif/button](https://components.espressif.com/components/espressif/button) component
- **Customizable Timeouts**: Configure different hold durations for Wi-Fi vs factory reset
- **Indicator Callbacks**: Visual/audio feedback when reset thresholds are reached

## Installation

Add the component to your project using:

```bash
idf.py add-dependency "espressif/app_reset^1.0.0"
```

Or add it to your `idf_component.yml`:

```yaml
dependencies:
  espressif/app_reset:
    version: "^1.0.0"
```

## Usage

### Basic Example

```c
#include <iot_button.h>
#include <button_gpio.h>
#include <app_reset.h>

#define BUTTON_GPIO          0
#define BUTTON_ACTIVE_LEVEL  0

void app_driver_init()
{
    /* Create button using app_reset helper */
    button_handle_t btn_handle = app_reset_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);

    if (btn_handle) {
        /* Register Wi-Fi reset (3 seconds) and Factory reset (10 seconds) */
        app_reset_button_register(btn_handle, 3, 10);
    }
}
```

### Advanced Example (with app-specific callbacks)

```c
#include <iot_button.h>
#include <button_gpio.h>
#include <app_reset.h>

#define BUTTON_GPIO          0
#define BUTTON_ACTIVE_LEVEL  0

static void my_button_callback(void *arg, void *data)
{
    /* Toggle LED, etc. */
}

void app_driver_init()
{
    /* Manually create button for more control */
    button_config_t btn_cfg = {
        .long_press_time = 0,
        .short_press_time = 0,
    };
    button_gpio_config_t gpio_cfg = {
        .gpio_num = BUTTON_GPIO,
        .active_level = BUTTON_ACTIVE_LEVEL,
        .enable_power_save = false,
    };
    button_handle_t btn_handle = NULL;

    if (iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn_handle) == ESP_OK && btn_handle) {
        /* Register your app-specific callback (single click) */
        iot_button_register_cb(btn_handle, BUTTON_SINGLE_CLICK, NULL, my_button_callback, NULL);

        /* Register Wi-Fi reset (3s) and Factory reset (10s) on the same button */
        app_reset_button_register(btn_handle, 3, 10);
    }
}
```

## API Reference

### `app_reset_button_create()`

Create a button handle with default configuration.

```c
button_handle_t app_reset_button_create(gpio_num_t gpio_num, uint8_t active_level);
```

**Parameters:**
- `gpio_num`: GPIO pin number for the button
- `active_level`: Active level (0 for active low, 1 for active high)

**Returns:** Button handle or NULL on error

### `app_reset_button_register()`

Register Wi-Fi and/or factory reset callbacks on a button.

```c
esp_err_t app_reset_button_register(button_handle_t btn_handle,
                                     uint8_t wifi_reset_timeout,
                                     uint8_t factory_reset_timeout);
```

**Parameters:**
- `btn_handle`: Button handle from `iot_button_create()` or `app_reset_button_create()`
- `wifi_reset_timeout`: Time in seconds to trigger Wi-Fi reset (0 to disable)
- `factory_reset_timeout`: Time in seconds to trigger factory reset (0 to disable)

**Returns:** `ESP_OK` on success

**Note:** It's recommended that `factory_reset_timeout > wifi_reset_timeout`.

## Behavior

When using both Wi-Fi and factory reset on the same button:

1. **Short Press (< wifi_reset_timeout)**: No reset action
2. **Hold for wifi_reset_timeout seconds**:
   - Indication callback fires: "Release now for Wi-Fi reset"
   - On release: Wi-Fi credentials cleared, device reboots
3. **Continue holding past factory_reset_timeout seconds**:
   - Indication callback fires: "Release for factory reset"
   - On release: Complete NVS erase, device reboots

## Customization

The component provides default indicator callbacks. To customize them, copy the source files to your project and modify:

- `wifi_reset_indicate()`: Called when Wi-Fi reset threshold is reached
- `factory_reset_indicate()`: Called when factory reset threshold is reached

## Dependencies

- ESP-IDF >= 5.1
- [espressif/button](https://components.espressif.com/components/espressif/button) ^4.1.4
- [espressif/rmaker_common](https://components.espressif.com/components/espressif/rmaker_common) (for ESP RainMaker projects)

## License

Apache License 2.0

