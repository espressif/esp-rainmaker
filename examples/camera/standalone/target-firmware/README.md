# Target (co-processor) firmware

On the ESP32-P4 dual-chip boards, the standalone camera example can flash the
attached ESP32-C6/C5 co-processor **in-system** — the P4 host writes the
co-processor image (packed here into a `slave` SPIFFS partition) over the UART
link, so you don't need to attach a separate cable/console to the C6/C5.

This only applies to the P4 board overlays that enable it (`p4_eye`, `p4x_eye`,
`p4_c5_core_board`). The single-chip ESP32/ESP32-S3 builds and the P4 kits with
a separate C6 USB port (`p4_function_ev_board_*`) don't use it — flash the
co-processor directly there.

## How it's used

When `CONFIG_SLAVE_FLASHER_ENABLE=y`, this example's `CMakeLists.txt` packs this
directory into a `slave` SPIFFS partition and the app calls `flash_slave()`
(from the `slave_flasher` component) at startup, which programs the C6/C5. The
build **fails fast** if the images below are missing, so you can't accidentally
flash an empty partition.

## Files to place here

| File | What it is |
|------|------------|
| `bootloader.bin`      | C6/C5 second-stage bootloader |
| `partition-table.bin` | C6/C5 partition table |
| `app.bin`             | C6/C5 application image |

These are **not** checked into the repo (see `.gitignore`).

## Where to build them from

The co-processor runs the `network_adapter` example (Wi-Fi/BT passthrough over
SDIO). Build it for the co-processor target and copy the outputs here (rename
the app image to `app.bin`):

```bash
cd $KVS_SDK_PATH/examples/network_adapter
idf.py set-target esp32c6          # or esp32c5 for p4_c5_core_board
idf.py build
cp build/bootloader/bootloader.bin        <this-dir>/bootloader.bin
cp build/partition_table/partition-table.bin <this-dir>/partition-table.bin
cp build/network_adapter.bin              <this-dir>/app.bin
```
