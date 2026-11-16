#!/usr/bin/env bash
# Builds the firmware and puts the one-file image to flash at offset 0x0 in dist/.
set -euo pipefail

cd "$(dirname "$0")/.."

# A pio on PATH first, as on GitHub's build machines; otherwise the copy PlatformIO installs for
# itself, as on a PC set up through the editor.
pio="$(command -v pio || echo "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/penv/bin/pio")"
"$pio" run -e sticky

# PlatformIO merges these into firmware.factory.bin:
#   0x0      bootloader.bin
#   0x8000   partitions.bin
#   0xe000   boot_app0.bin  selects app0, in case an earlier firmware last ran from app1
#   0x10000  firmware.bin
mkdir -p dist
cp .pio/build/sticky/firmware.factory.bin dist/pocketplay-full.bin

echo
echo "Ready to flash: $(pwd)/dist/pocketplay-full.bin"
