# Matter + Rainmaker Examples

> Note: Works only with RainMaker iOS App v3.0.0 or later

## Prerequisites

- ESP-IDF [v5.4.1](https://github.com/espressif/esp-idf/releases/v5.4.1)
- [Espressif's SDK for Matter](https://github.com/espressif/esp-matter).
- [ESP Rainmaker SDK](https://github.com/espressif/esp-rainmaker)
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
So, we will use [host driven claiming](https://docs.rainmaker.espressif.com/docs/product_overview/concepts/claiming#host-driven-claiming-publicprivate-rainmaker) via the [RainMaker CLI](https://github.com/espressif/esp-rainmaker-cli/blob/master/docs/README.md#installation).

Make sure your device is connected to the host machine, login into the CLI and execute this:

```
$ esp-rainmaker-cli claim --matter <port>
```

This will fetch the device certificates and flash them on your device.

### Generating the factory nvs binary

The factory nvs (fctry partition) needs to be generated using the [esp-matter-mfg-tool](https://github.com/espressif/esp-matter-tools).

It is released on pypi: https://pypi.org/project/esp-matter-mfg-tool and can be installed by running `pip install esp-matter-mfg-tool`

```
$ esp-matter-mfg-tool --vendor-id 0x131B --product-id 0x2 \
                      --vendor-name "Espressif" --product-name "RainMaker-Matter-Light" \
                      --hw-ver-str "DevKitM1" \
                      -cd $RMAKER_PATH/examples/matter/mfg/cd_131B_0002.der \
                      --csv $RMAKER_PATH/examples/matter/mfg/keys.csv \
                      --mcsv $RMAKER_PATH/examples/matter/mfg/master.csv
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
The QR Code required for commissioning your device can be found at the directory where esp-matter-mfg-tool command is executed or argument provided to `--outdir`.
For e.g: If the command is run in esp-rainmaker directory , the QR code will be located at `${RMAKER_PATH}/out/<vendor-id>_<product-id>/<node-id>/<node-id>-qrcode.png`

## Manufacturing Considerations

This step is only suggested for Privately deployed Production and not required for test set up.

### RainMaker MQTT Host

Find your private deployment's mqtt hostname (if applicable) by sending a GET request at `https://<Rainmaker-API-endpoint>/mqtt_host`. You should replace the mqtt host in master.csv (As described in the [section](#generating-the-factory-nvs-binary) above) with this to generate the factory nvs binary.

### Matter vid/pid

For production devices which may have a different matter vid and pid, please set the values of `DEVICE_VENDOR_ID` and `DEVICE_PRODUCT_ID` by using `idf.py menuconfig -> Component config > CHIP Device Layer > Device Identification Options`. These same should also be used in the mfg_tool.

### Matter DAC

For public RainMaker, some test DACs are provided via claiming. For private deployments, test DACs can be generated using esp-matter-mfg-tool.

```
$ esp-matter-mfg-tool -v <vendor-id> -p <product-id> --pai -k <pai-key> -c <pai-cert> -cd <cert-dclrn> --csv /path/to/keys.csv --mcsv /path/to/master.csv
```

Samples of keys.csv and master.csv can be found in $RMAKER_PATH/examples/matter/mfg/.

For testing, you can use the test vid, pid, PAI and CD as shown below

```
$ esp-matter-mfg-tool --dac-in-secure-cert -v 0xFFF2 -p 0x8001 --pai -k $ESP_MATTER_PATH/connectedhomeip/connectedhomeip/credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Key.pem -c $ESP_MATTER_PATH/connectedhomeip/connectedhomeip/credentials/test/attestation/Chip-Test-PAI-FFF2-8001-Cert.pem -cd $ESP_MATTER_PATH/connectedhomeip/connectedhomeip/credentials/test/certification-declaration/Chip-Test-CD-FFF2-8001.der --csv $RMAKER_PATH/examples/matter/mfg/keys.csv --mcsv $RMAKER_PATH/examples/matter/mfg/master.csv
```

Note the path where the files are generated after running the above command since it will be required later.

The csv file generate at `/out/<vendor-id>_<product-id>/cn_dacs-<date>-<time>.csv` should be registered to your private RainMaker deployment (if applicable) using the steps mentioned [here](https://github.com/espressif/esp-rainmaker-admin-cli#register-device-certificates).

> **In production use case, the DACs will be pre-provisioned in the modules and a csv file will be provided by the Espressif factory directly. Optionally, even the fctry partitions can be pre programmed. If not, use the mfg_tool to generate these nvs binaries**
