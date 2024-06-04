# Matter + RainMaker Light Example over Thread

This section is the instruction of setting up the Matter + RainMaker Light example on Thread platforms(ESP32-H2 or ESP32-C6).

## Prerequisites

- ESP-IDF [v5.1.2](https://github.com/espressif/esp-idf/tree/v5.1.2)
- [ESP-Thread-BR SDK](https://github.com/espressif/esp-thread-br)
- [ESP-Matter SDK](https://github.com/espressif/esp-matter)
- [ESP Rainmaker SDK](https://github.com/espressif/esp-rainmaker)
- [ESP Secure Cert Manager](https://github.com/espressif/esp_secure_cert_mgr)

## Thread BR (Border Router) with NAT64 (Network Address Translation) feature

To communicate with the RainMaker cloud server, Thread end devices must be part of a Thread Network which incorporates a Thread BR with the NAT64 feature enabled. The two following Thread BRs are recommended:

- Apple HomePod (or HomePod mini)

To use HomePod as the Thread BR for your Matter+RainMaker Thread device, the HomePod must firstly be added to your Apple Home app.

- ESP-Thread-BR

The [Thread BR example](https://github.com/espressif/esp-thread-br/tree/main/examples/basic_thread_border_router) enables NAT64 feature by default. Please follow the [README](https://github.com/espressif/esp-thread-br/blob/main/examples/basic_thread_border_router/README.md) to set up the Thread BR and form a Thread network.

## Setting up the environment

Please following the [README](../README.md) to set up the environment.

For ESP32-C6, the [sdkconfig file](./sdkconfig.esp32c6.thread) for Thread is provided.
```
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.esp32c6.thread" set-target esp32c6 build
```

## Pairing the Thread devices to RainMaker Fabric

- Apple HomePod

If you are using HomePod as the Thread BR, you can directly use the RainMaker iOS APP to commission the Thread device to the Thread network with Matter BLE-Thread pairing. Scan the QR code and the phone APP will finish the commissioning process.

- ESP-Thread-BR

Currently the RainMaker iOS APP cannot pairing a Thread device to the Thread network formed by ESP-Thread-BR. The device should be commisioned with chip-tool and you should open commissioning window with chip-tool to enable Matter on-network commissioning. Then the phone app can add the device to RainMaker Fabric with Matter on-network pairing.
```
cd $ESP_MATTER_PATH/connectedhomeip/connectedhomeip/out/host/
./chip-tool pairing ble-thread <node-id> <thread-dataset-tlvs> <passcode> <discriminator> --paa-trust-store-path <PAA path>
./chip-tool pairing open-commissioning-window <node-id> 1 <timeout> <iterations> <discriminator>
```

After commissioning the device, you can try to controller the device with either Matter protocal(local) or RainMaker cloud(remote).
