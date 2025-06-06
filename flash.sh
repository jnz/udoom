#!/bin/bash
set -e

FIRMWARE_BIN="firmware.bin"
FIRMWARE_HEX="firmware.hex"

if command -v STM32_Programmer_CLI &> /dev/null; then
    STM32_Programmer_CLI -c port=SWD mode=UR -d "$FIRMWARE_HEX" -rst
else
    echo "STM32_Programmer_CLI not found, trying st-flash instead..."
    echo "Using st-flash to internal flash at $FLASH_ADDR..."
    FLASH_ADDR="${FLASH_ADDR:-0x8000000}"
    st-flash write "$FIRMWARE_BIN" "$FLASH_ADDR"
fi

echo "=== Flash complete ==="

