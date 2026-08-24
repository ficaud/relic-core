#!/usr/bin/env bash
# ============================================================================
# host-flash.sh – Flash ESP32-S3 or ESP32 from macOS host
# ============================================================================
# Usage:
#
#   ./tools/host-flash.sh                           # interactive board choice
#   ./tools/host-flash.sh esp32s3                   # flash ESP32-S3
#   ./tools/host-flash.sh esp32                     # flash ESP32 (WROOM-32)
#   ./tools/host-flash.sh xiao                      # flash Seeed XIAO ESP32S3
#   ./tools/host-flash.sh esp32s3 path/to/firmware.bin  # custom binary
#
# The script uses esptool directly on the Mac's serial port.
# Default binary paths (per platform):
#   ESP32-S3     → <project>/build-esp32s3/zephyr/zephyr.bin
#   ESP32        → <project>/build-esp32wroom/zephyr/zephyr.bin
#   XIAO ESP32S3 → <project>/build-xiao/zephyr/zephyr.bin
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

# ── Board definitions ──
BOARD_ESP32S3="esp32s3"
BOARD_ESP32="esp32"
BOARD_XIAO="xiao"

# ── Parse board choice ──
BOARD=""
USER_BIN=""

if [ $# -ge 1 ]; then
  case "$1" in
    "$BOARD_ESP32S3"|"$BOARD_ESP32"|"$BOARD_XIAO")
      BOARD="$1"
      USER_BIN="${2:-}"
      ;;
    *)
      USER_BIN="$1"
      ;;
  esac
fi

# ── Interactive board selection ──
if [ -z "$BOARD" ]; then
  echo "🔎 Select the target board:"
  echo "   1) ESP32-S3 (ESP32-S3-DevKitC-1)"
  echo "   2) ESP32   (ESP32-WROOM-32 / DOIT ESP32 DevKit V1)"
  echo "   3) XIAO    (Seeed XIAO ESP32S3)"
  read -rp "Choice [1/2/3]: " CHOICE
  case "$CHOICE" in
    1) BOARD="$BOARD_ESP32S3" ;;
    2) BOARD="$BOARD_ESP32" ;;
    3) BOARD="$BOARD_XIAO" ;;
    *)
      echo "❌ Invalid choice: $CHOICE"
      exit 1
      ;;
  esac
fi

# ── Board-specific parameters ──
case "$BOARD" in
  "$BOARD_ESP32S3"|"$BOARD_XIAO")
    CHIP="esp32s3"
    BAUD=921600
    FLASH_OFFSET="0x0"
    case "$BOARD" in
      "$BOARD_ESP32S3") BUILD_DIR="$SCRIPT_DIR/build-esp32s3" ;;
      "$BOARD_XIAO")    BUILD_DIR="$SCRIPT_DIR/build-xiao" ;;
    esac
    ;;
  "$BOARD_ESP32")
    CHIP="esp32"
    BAUD=460800
    FLASH_OFFSET="0x1000"
    BUILD_DIR="$SCRIPT_DIR/build-esp32wroom"
    ;;
esac

# ── Serial port detection ──
case "$BOARD" in
  "$BOARD_ESP32S3"|"$BOARD_XIAO")
    PORT_PATTERN="/dev/cu.usbmodem*"
    ;;
  "$BOARD_ESP32")
    PORT_PATTERN="/dev/cu.usbserial*"
    ;;
esac

PORT=$(ls $PORT_PATTERN 2>/dev/null | head -1)

if [ -z "$PORT" ]; then
  echo "❌ No ESP32 port found (looked for $PORT_PATTERN)"
  echo "   Plug in the board and check with: ls $PORT_PATTERN"
  exit 1
fi

echo "🔌 Detected port: $PORT"
echo "🎯 Board: $BOARD ($CHIP, baud $BAUD, offset $FLASH_OFFSET)"

# ── Ensure esptool is available ──
if ! python3 -c "import esptool" 2>/dev/null; then
  echo "📦 Installing esptool…"
  pip3 install esptool
fi

# ── Binary to flash ──
if [ -n "$USER_BIN" ]; then
  BIN="$USER_BIN"
else
  BIN="$BUILD_DIR/zephyr/zephyr.bin"
fi

if [ ! -f "$BIN" ]; then
  echo "❌ Binary not found: $BIN"
  echo "   Build first, or provide a path: $0 $BOARD path/to/firmware.bin"
  exit 1
fi

echo "⚡ Flashing $BIN …"
echo "   Hold BOOT, tap RESET, release BOOT."
esptool --chip "$CHIP" --port "$PORT" --baud "$BAUD" \
  --before default-reset --after hard-reset \
  write-flash "$FLASH_OFFSET" "$BIN"
