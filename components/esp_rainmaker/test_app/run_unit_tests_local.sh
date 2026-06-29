#!/bin/bash
# Run CI unit-test flow locally using resource-pool-cli for hardware.
# Same logic as CI: grab device, claiming (login + claim + flash claim data), build, flash FW, run tests.
#
# Prerequisites:
#   - ESP-IDF in PATH (source export.sh)
#   - For claiming: UT_USERNAME, UT_PASSWORD, MAC_ADDR
#   - Optional: TARGET (default esp32c3)
#
# Usage:
#   ./components/esp_rainmaker/test_app/run_unit_tests_local.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
cd "$REPO_ROOT"

if ! command -v idf.py >/dev/null 2>&1; then
    echo "ERROR: idf.py not found in PATH."
    echo "Please source ESP-IDF first:"
    echo "  source \$IDF_PATH/export.sh"
    exit 1
fi

# For resource pool; CI uses CI_JOB_ID
JOB_ID="${JOB_ID:-local_$$}"
TARGET="${TARGET:-esp32c3}"

RAINMAKER_LOGGED_IN=""
cleanup() {
  if [ -f "$REPO_ROOT/.resource_id" ] && [ -f "$REPO_ROOT/.job_id" ]; then
    echo "Releasing resource..."
    python3 "$REPO_ROOT/resource-pool-cli.py" release --resource-id "$(cat "$REPO_ROOT/.resource_id")" --user-id "$(cat "$REPO_ROOT/.job_id")" || true
    rm -f "$REPO_ROOT/.resource_id" "$REPO_ROOT/.job_id"
  fi
  if [ -n "$RAINMAKER_LOGGED_IN" ] && command -v esp-rainmaker-cli &>/dev/null; then
    esp-rainmaker-cli logout &>/dev/null || true
  fi
}
trap cleanup EXIT

echo "=== 1. Download resource-pool-cli ==="
curl -o "$REPO_ROOT/resource-pool-cli.py" "pn-tools.espressif.cn:5011/download-cli"
python3 -m pip install -q paramiko scp esptool

echo "=== 2. Grab device from resource pool ==="
RESOURCE_OUTPUT=$(python3 "$REPO_ROOT/resource-pool-cli.py" grab --user-id "$JOB_ID" --resource-type "$TARGET" --wait-time 60 --timeout 3600)
echo "$RESOURCE_OUTPUT"
# Portable field extraction (sed -nE works on both GNU and BSD/macOS; grep -oP / \s do not)
RESOURCE_ID=$(echo "$RESOURCE_OUTPUT" | sed -nE 's/^[[:space:]]*resource_id:[[:space:]]*([^[:space:]]+).*/\1/p' | head -1)
# Record resource and job id immediately so cleanup EXIT trap can release even if later steps fail
echo "$RESOURCE_ID" > "$REPO_ROOT/.resource_id"
echo "$JOB_ID" > "$REPO_ROOT/.job_id"
DEVICE_IP=$(echo "$RESOURCE_OUTPUT" | sed -nE 's/^[[:space:]]*ip:[[:space:]]*([^[:space:]]+).*/\1/p' | head -1)
RFC_PORT=$(echo "$RESOURCE_OUTPUT" | sed -nE 's/^[[:space:]]*rfc_port:[[:space:]]*([^[:space:]]+).*/\1/p' | head -1)
RESOURCE_CONFIG_FILE=$(echo "$RESOURCE_OUTPUT" | sed -nE 's/.*saved to:[[:space:]]*([^[:space:]]+).*/\1/p' | head -1)
export DEVICE_PORT="rfc2217://${DEVICE_IP}:${RFC_PORT}"
export RESOURCE_CONFIG_FILE
echo "Device: $DEVICE_PORT | Config: $RESOURCE_CONFIG_FILE"

echo "=== 3. RainMaker claiming (login, claim, prepare claim data) ==="
if [ -z "${UT_USERNAME}" ] || [ -z "${UT_PASSWORD}" ]; then
  echo "ERROR: UT_USERNAME and UT_PASSWORD are required for claiming. Set them and re-run."
  exit 1
fi
if [ -z "${MAC_ADDR}" ]; then
  echo "ERROR: MAC_ADDR is required for claiming (device MAC, e.g. 7C2C672AEE84). Set it and re-run."
  exit 1
fi
python3 -m pip install -q esp-rainmaker-cli
esp-rainmaker-cli profile switch global
if esp-rainmaker-cli login --user_name "${UT_USERNAME}" --password "${UT_PASSWORD}"; then
  RAINMAKER_LOGGED_IN=1
else
  echo "Login failed; continuing without claim data."
fi
if [ -n "$RAINMAKER_LOGGED_IN" ]; then
  esp-rainmaker-cli claim --mac "${MAC_ADDR}" --outdir "$REPO_ROOT/.rainmaker_claim" || { echo "Claim failed; continuing without claim data."; }
fi
export CLAIM_DATA_PATH=$(find "$REPO_ROOT/.rainmaker_claim" -type d -name "${MAC_ADDR}" 2>/dev/null | head -n 1)
if [ -n "${CLAIM_DATA_PATH}" ] && [ -f "${CLAIM_DATA_PATH}/node.info" ]; then
  export NODE_ID=$(cat "${CLAIM_DATA_PATH}/node.info")
  echo "Claim data ready: $NODE_ID"
else
  echo "No claim data; tests will run without claimed credentials."
fi

echo "=== 4. Build test app ==="
cd "$REPO_ROOT/components/esp_rainmaker/test_app"
idf.py set-target "$TARGET"
idf.py build

echo "=== 5. Flash firmware ==="
python3 "$REPO_ROOT/resource-pool-cli.py" erase-flash "$RESOURCE_CONFIG_FILE"
if [ "$TARGET" = "esp32" ]; then
  BOOTLOADER_ADDR="0x1000"
else
  BOOTLOADER_ADDR="0x0"
fi
python3 "$REPO_ROOT/resource-pool-cli.py" flash build/bootloader/bootloader.bin "$BOOTLOADER_ADDR" "$RESOURCE_CONFIG_FILE"
python3 "$REPO_ROOT/resource-pool-cli.py" flash build/partition_table/partition-table.bin 0x8000 "$RESOURCE_CONFIG_FILE"
python3 "$REPO_ROOT/resource-pool-cli.py" flash build/ota_data_initial.bin 0x16000 "$RESOURCE_CONFIG_FILE"
python3 "$REPO_ROOT/resource-pool-cli.py" flash build/esp_rainmaker_test.bin 0x20000 "$RESOURCE_CONFIG_FILE"
FCTRY_OFFSET="0x3FA000"
if [ -n "${CLAIM_DATA_PATH}" ] && [ -f "${CLAIM_DATA_PATH}/${MAC_ADDR}.bin" ]; then
  echo "Flashing RainMaker claim data to $FCTRY_OFFSET (fctry partition)..."
  python3 "$REPO_ROOT/resource-pool-cli.py" flash "${CLAIM_DATA_PATH}/${MAC_ADDR}.bin" "$FCTRY_OFFSET" "$RESOURCE_CONFIG_FILE"
else
  echo "Skipping claim data flash (no claim data)."
fi

echo "=== 6. Start RFC2217 server (background) ==="
python3 "$REPO_ROOT/resource-pool-cli.py" rfc2217-server "$RESOURCE_CONFIG_FILE" &
RFC_PID=$!
sleep 3

echo "=== 7. Run unit tests ==="
python3 "$SCRIPT_DIR/run_test.py" "$DEVICE_PORT" -l "Enter next test" --log-file result.txt || true
kill $RFC_PID 2>/dev/null || true

echo "=== 8. Check result ==="
# Match a Unity FAIL line — including the line-terminal ":FAIL" form (no trailing colon).
# Kept identical to the CI gate in .gitlab-ci.yml.
if grep -qE ':FAIL($|[: ])' result.txt; then
  echo "❌ Found FAIL entries in result.txt"
  exit 1
fi
echo "✅ No FAIL entries found"
