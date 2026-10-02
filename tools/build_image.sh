#!/usr/bin/env bash
# Builds the CYD firmware and merges bootloader, partition table, boot_app0
# and app into ONE image (flash at address 0x0), for flashing without
# PlatformIO (esptool or a browser flasher), see FLASHEN.md.
#
#   tools/build_image.sh          -> dist/haltestellenanzeige-cyd.bin
#
# The image contains the API token from include/secrets.h: share it only
# with the workshop team, never publish it. dist/ is git-ignored.
set -euo pipefail
cd "$(dirname "$0")/.."

pio run -s -e cyd
mkdir -p dist
BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

# esptool via PlatformIO, so it runs with PlatformIO's own Python.
pio pkg exec --package tool-esptoolpy -- esptool.py --chip esp32 merge_bin -o dist/haltestellenanzeige-cyd.bin \
  --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000 .pio/build/cyd/bootloader.bin \
  0x8000 .pio/build/cyd/partitions.bin \
  0xe000 "$BOOT_APP0" \
  0x10000 .pio/build/cyd/firmware.bin >/dev/null

ls -l dist/haltestellenanzeige-cyd.bin
