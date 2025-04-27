# Matter + Rainmaker Examples

> Note: Works only with RainMaker iOS App v3.0.0 or later

## Prerequisites

- ESP-IDF [v5.2.2](https://github.com/espressif/esp-idf/releases/v5.2.2)
- [ESP-Matter SDK](https://github.com/espressif/esp-matter). Latest known working commit is [8a7c81f2](https://github.com/espressif/esp-matter/tree/8a7c81f28b8d787247d42b0992ae264555bec936).
- [ESP Rainmaker SDK](https://github.com/espressif/esp-rainmaker)
- [ESP Secure Cert Manager](https://github.com/espressif/esp_secure_cert_mgr)
Please go through the installation process (if required) for all of the above prerequisites.

## Setting up the environment
For building, the example you need to setup the IDF and ESP-Matter environment and also set the RainMaker path

```
$ cd /path/to/esp-idf
$ . ./export.sh
$ cd /path/to/esp-matter
$ . ./export.sh
$ export RMAKER_PATH=/path/to/esp-rainmaker
```

### Claiming device certificates

Self Claiming or Assisted Claiming can't be used with the RainMaker + Matter examples because the certificate needs to be present even before Matter commissioning starts.
So, we will use [host driven claiming](https://rainmaker.espressif.com/docs/claiming#host-driven-claiming) via the [RainMaker CLI](https://rainmaker.espressif.com/docs/cli-setup).

Make sure your device is connected to the host machine, login into the CLI and execute this:

```
$ esp-rainmaker-cli claim --matter <port>
```

This will fetch the device certificates and flash them on your device.

### Generating the factory nvs binary

The factory nvs (fctry partition) needs to be generated using the mfg_tool of esp-matter

mfg_tool is moved to esp-matter-tools repo: https://github.com/espressif/esp-matter-tools/tree/main/mfg_tool.

It is released on pypi: https://pypi.org/project/esp-matter-mfg-tool and can be installed by running `pip install esp-matter-mfg-tool`

```
$ export ESP_SECURE_CERT_PATH=/path/to/esp_secure_cert_mgr
$ esp-matter-mfg-tool -v 0x131B -p 0x2 -cd $RMAKER_PATH/examples/matter/mfg/cd_131B_0002.der --csv $RMAKER_PATH/examples/matter/mfg/keys.csv --mcsv $RMAKER_PATH/examples/matter/mfg/master.csv
```

This not only generates the factory nvs binary required for matter, but also embeds the RainMaker MQTT Host url into it via the master.csv file. Optionally, you can embed the MQTT host into the firmware itself by using `idf.py menuconfig -> ESP RainMaker Config -> ESP_RMAKER_READ_MQTT_HOST_FROM_CONFIG` and then skipping the --csv and --mcsv options to mfg_tool

The factory binary generated above should be flashed onto the fctry partition (default : `0x3f9000` for ESP32-C6 and `0x3e0000` for other chips. Do check your partition table for exact address).

```
$ esptool.py write_flash 0x3e0000 out/131b_2/<node-id>/<node-id>-partition.bin
```

## Building the example

Once the environment and required files are set up, we can now proceed to build and flash the example

```
$ cd <example-directory>
$ idf.py set-target esp32c3
$ idf.py build
$ idf.py flash monitor
```

### Commissioning
The QR Code required for commissioning your device can be found at `${ESP_MATTER_PATH}/tools/mfg_tool/out/<vendor-id>_<product-id>/<node-id>/<node-id>-qrcode.png`


## Manufacturing Considerations

This step is only suggested for Privately deployed Production and not required for test set up.

### RainMaker MQTT Host

Find your private deployment's mqtt hostname (if applicable) by sending a GET request at `https://<Rainmaker-API-endpoint>/mqtt_host`. You should replace the mqtt host in master.csv (As described in the [section](#generating-the-factory-nvs-binary) above) with this to generate the factory nvs binary.

### Matter vid/pid

For production devices which may have a different matter vid and pid, please set the values of `DEVICE_VENDOR_ID` and `DEVICE_PRODUCT_ID` by using `idf.py menuconfig -> Component config > CHIP Device Layer > Device Identification Options`. These same should also be used in the mfg_tool.

### Matter DAC

For public RainMaker, some test DACs are provided via claiming. For private deployments, test DACs can be generated using mfg_tool

mfg_tool is moved to esp-matter-tools repo: https://github.com/espressif/esp-matter-tools/tree/main/mfg_tool.

It is released on pypi: https://pypi.org/project/esp-matter-mfg-tool and can be installed by running `pip install esp-matter-mfg-tool`

```
$ export ESP_SECURE_CERT_PATH=/path/to/esp_secure_cert_mgr
$ esp-matter-mfg-tool -v <vendor-id> -p <product-id> --pai -k <pai-key> -c <pai-cert> -cd <cert-dclrn> --csv /path/to/keys.csv --mcsv /path/to/master.csv
```

Samples of keys.csv and master.csv can be found in $RMAKER_PATH/examples/matter/mfg/.


For testing, you can use the test vid, pid, PAI and CD as shown below

```
$ esp-matter-mfg-tool --dac-in-secure-cert -v 0xFFF2 -p 0x8001 --pai -k $ESP_MATTER_PATH/connectedhomeip/connectedhomeip/credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Key.pem -c $ESP_MATTER_PATH/connectedhomeip/connectedhomeip/credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Cert.pem -cd $ESP_MATTER_PATH/connectedhomeip/connectedhomeip/credentials/test/certification-declaration/Chip-Test-CD-FFF2-8001.der --csv $RMAKER_PATH/examples/matter/mfg/keys.csv --mcsv $RMAKER_PATH/examples/matter/mfg/master.csv
```

Note the path where the files are generated after running the above command since it will be required later.

### Configure your app
Open the project configuration menu using -

```
idf.py menuconfig
```
In the configuration menu, set the following additional configuration to use custom factory partition and different values for Data and Device Info Providers.

1. Enable `ESP32 Factory Data Provider` [Component config → CHIP Device Layer → Commissioning options → Use ESP32 Factory Data Provider]

    Enable config option `CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER`
    to use ESP32 specific implementation of CommissionableDataProvider and DeviceAttestationCredentialsProvider.

2. Enable `ESP32 Device Instance Info Provider` [Component config → CHIP Device Layer → Commissioning options → Use ESP32 Device Instance Info Provider]

    Enable config option `ENABLE_ESP32_DEVICE_INSTANCE_INFO_PROVIDER`
    to get device instance info from factory partition.

3. Enable `Attestation - Secure Cert` [ Component config → ESP Matter → DAC Provider options → Attestation - Secure Cert]

    Enable config option `CONFIG_FACTORY_PARTITION_DAC_PROVIDER` to use DAC certificates from the secure_cert partition during Attestation.

4. Set `chip-factory namespace partition label` [Component config → CHIP Device Layer → Matter Manufacturing Options → chip-factory namespace partition label]

    Set config option `CHIP_FACTORY_NAMESPACE_PARTITION_LABEL`
    to choose the label of the partition to store key-values in the "chip-factory" namespace. The default chosen partition label is `nvs`, change it to `fctry`.


Connect your esp32 device to your computer. Enter the below command to flash certificates and factory partition
```
$ esptool.py write_flash 0xd000 /out/<vendor-id>_<product-id>/<node-id>/<node-id>_esp_secure_cert.bin 0x3e0000 ./out/<vendor-id>_<product-id>/<node-id>/<node-id>-partition.bin
```

The csv file generate at `/out/<vendor-id>_<product-id>/cn_dacs-<date>-<time>.csv` should be registered to your private RainMaker deployment (if applicable) using the steps mentioned [here](https://github.com/espressif/esp-rainmaker-admin-cli#register-device-certificates).

> **In production use case, the DACs will be pre-provisioned in the modules and a csv file will be provided by the Espressif factory directly. Optionally, even the fctry partitions can be pre programmed. If not, use the mfg_tool to generate these nvs binaries**
