#!/bin/bash
set -e

FIRMWARE_BIN="bootloader_precompiled.bin"
FIRMWARE_HEX="bootloader_precompiled.hex"

if command -v STM32_Programmer_CLI &> /dev/null; then
    STM32_Programmer_CLI -c port=SWD -d "$FIRMWARE_HEX" -rst
else
    echo "STM32_Programmer_CLI not found, trying st-flash instead..."
    echo "Using st-flash to internal flash at $FLASH_ADDR..."
    st-flash write "$FIRMWARE_BIN" "$FLASH_ADDR"
fi

echo "=== Flash complete ==="

