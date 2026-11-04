#!/usr/bin/env bash
# Builds the firmware and puts the one-file image to flash at offset 0x0 in dist/.
set -euo pipefail

cd "$(dirname "$0")/.."

"${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/penv/bin/pio" run -e sticky

# PlatformIO merges these into firmware.factory.bin:
#   0x0      bootloader.bin
#   0x8000   partitions.bin
#   0xe000   boot_app0.bin  selects app0, in case an earlier firmware last ran from app1
#   0x10000  firmware.bin
mkdir -p dist
cp .pio/build/sticky/firmware.factory.bin dist/boardbyte-full.bin

echo
echo "Ready to flash: $(pwd)/dist/boardbyte-full.bin"
