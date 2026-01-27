# Ethernet Switch Example

This example demonstrates how to use ESP RainMaker with Ethernet connectivity. Wi-Fi support can be optionally enabled for dual-network operation.

## Features

- Ethernet-based network connectivity (primary)
- Optional Wi-Fi support (enable via `EXAMPLE_ENABLE_WIFI` Kconfig option)
- On-network challenge-response for user-node mapping (Ethernet) or Wi-Fi provisioning (when Wi-Fi enabled)
- Switch device with button and LED control
- All standard RainMaker features (OTA, scheduling, scenes, etc.)
- **Dual network support**: When both Wi-Fi and Ethernet are enabled, RainMaker uses whichever connects first

## Hardware Requirements

- ESP32 board with Ethernet support (e.g., ESP32 with external PHY chip)
- Ethernet PHY chip (e.g., LAN8720, IP101, etc.)
- Proper GPIO connections for Ethernet (MDC, MDIO, RMII pins, etc.)

## Build and Flash

1. Configure Ethernet GPIO pins in `menuconfig`:
   - Go to `Example Ethernet Configuration` menu
   - Set MDC, MDIO, PHY address, reset GPIO, and RMII pins as per your hardware

2. (Optional) Enable Wi-Fi support:
   - Go to `Example Configuration` menu
   - Enable `EXAMPLE_ENABLE_WIFI`
   - Also enable Wi-Fi in `Component config -> Wi-Fi -> [*] WiFi`
   - This enables Wi-Fi provisioning in addition to Ethernet

3. Build and flash:
   ```bash
   idf.py build flash monitor
   ```

## Provisioning

### Ethernet-Only Mode (Default)

When Wi-Fi is disabled, this example uses **on-network challenge-response** for user-node mapping:

1. Connect the device to your network via Ethernet
2. Wait for the device to get an IP address
3. Use esp-rainmaker-cli to discover and map the device:
   ```bash
   esp-rainmaker-cli provision --transport on-network
   ```

The device will be discoverable via mDNS once it gets an IP address.

### Dual Network Mode (Wi-Fi + Ethernet)

When Wi-Fi is enabled (`EXAMPLE_ENABLE_WIFI=y`):

- **Wi-Fi Provisioning**: Use the phone app to provision Wi-Fi credentials (same as other RainMaker examples)
- **Ethernet**: Connect via Ethernet cable - device will use Ethernet if available
- **RainMaker**: Uses whichever network connects first (Wi-Fi or Ethernet)
- **On-Network Challenge-Response**: Also available on both networks for user-node mapping

You can provision Wi-Fi first, then connect Ethernet, or vice versa. RainMaker will work with whichever network is available.

## Configuration

- Enable `CONFIG_ESP_RMAKER_ON_NETWORK_CHAL_RESP_ENABLE` in menuconfig (for Ethernet provisioning)
- Configure Ethernet GPIO pins based on your hardware
- (Optional) Enable `EXAMPLE_ENABLE_WIFI` for dual-network support
- The device must be claimed before it can be used (use `esp-rainmaker-cli claim`)

## What to expect

- The LED state (green color) indicates the state of the switch
- Pressing the BOOT button will toggle the state of the switch and hence the LED
- Toggling the switch on the phone app should toggle the LED on your board
