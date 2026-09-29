#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Flash ESP32-S3 firmware via remote OpenOCD (Raspberry Pi) over JTAG.
#
# Prerequisites:
#   - .env at project root with RPI_IP_ADDRESS set
#   - OpenOCD (Espressif fork) running on the RPI (port 3333)
#   - west build completed
#
# Usage:
#   ./tools/flash-esp32s3-jtag.sh
#   make flash-jtag

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# shellcheck source=/dev/null
source "${PROJECT_DIR}/.env"

# The ESP32-S3 firmware is built into build-esp32s3/ by `make build`. Allow
# override via BUILD_DIR env var.
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build-esp32s3}"
BIN="${BUILD_DIR}/zephyr/zephyr.bin"
ELF="${BUILD_DIR}/zephyr/zephyr.elf"
WRAPPER="${BUILD_DIR}/zephyr/zephyr-wrapper.bin"
WRAPPER_ELF="${BUILD_DIR}/zephyr/zephyr-wrapper.elf"
GDB="/opt/zephyr-sdk/gnu/xtensa-espressif_esp32s3_zephyr-elf/bin/xtensa-espressif_esp32s3_zephyr-elf-gdb"
GDB_SCRIPT="${SCRIPT_DIR}/flash-esp32s3-jtag.gdb"
OBJCOPY="/opt/zephyr-sdk/gnu/xtensa-espressif_esp32s3_zephyr-elf/bin/xtensa-espressif_esp32s3_zephyr-elf-objcopy"

# ------------------------------------------------------------------
# Check prerequisites
# ------------------------------------------------------------------
if [ ! -f "${BIN}" ]; then
    echo "Firmware not found at ${BIN}"
    echo "Run 'west build' first, or use the Build task."
    exit 1
fi

if [ -z "${RPI_IP_ADDRESS:-}" ]; then
    echo "RPI_IP_ADDRESS not set in .env"
    exit 1
fi

# ------------------------------------------------------------------
# Build the wrapper ELF (zephyr.bin → ELF with .data section at 0x0)
# ------------------------------------------------------------------
cp "${BIN}" "${WRAPPER}"

"${OBJCOPY}" \
    -I binary \
    -O elf32-xtensa-le \
    -B xtensa \
    "${WRAPPER}" \
    "${WRAPPER_ELF}"

# Patch e_type from REL (1) to EXEC (2) so GDB accepts it
python3 -c "
with open('${WRAPPER_ELF}', 'r+b') as f:
    d = bytearray(f.read())
    d[16] = 2
    f.seek(0)
    f.write(d)
"

# ------------------------------------------------------------------
# Flash via GDB + remote OpenOCD
# ------------------------------------------------------------------
echo "Flashing ESP32-S3 to ${RPI_IP_ADDRESS}:3333 ..."
"${GDB}" \
    -ex "target remote ${RPI_IP_ADDRESS}:3333" \
    -x "${GDB_SCRIPT}" \
    "${ELF}"

echo "Flash complete"
