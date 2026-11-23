#!/bin/sh
# Configure a bare Unified build for a target and write it to the app slot.
#   flash.sh <pio env> <targets.json path> <port> [binary_configurator options...]
set -e
cd "$(dirname "$0")/.."
ENV=$1; TARGET=$2; PORT=$3; shift 3
PY=~/.platformio/penv/Scripts/python.exe
OUT=.pio/build/$ENV/configured
mkdir -p "$OUT"
cp ".pio/build/$ENV/firmware.bin" "$OUT/firmware.bin"
python python/binary_configurator.py --dir . --target "$TARGET" "$@" "$OUT/firmware.bin" > "$OUT/configure.log" 2>&1 || { cat "$OUT/configure.log"; exit 1; }
python python/external/esptool/esptool.py --port "$PORT" --baud 921600 write_flash 0x10000 "$OUT/firmware.bin" 2>&1 | grep -E "Chip is|Wrote|Hash|rror"
