#!/bin/bash

set -e

FIRMWARE="firmware.hex"
QSPI_ADDR="0x90000000"

# STM32CubeProgrammer root install path (can override via env var)
STM32CLT_PATH="${STM32CLT_PATH:-/opt/stm32cube/stm32cubeprogrammer}"
PROGRAMMER_CLI="$STM32CLT_PATH/bin/STM32_Programmer_CLI"
EXTERNAL_LOADER="$STM32CLT_PATH/bin/ExternalLoader/N25Q128A_STM32F7508-DISCO.stldr"

echo "=== Flash to ST-LINK QSPI Under Reset Mode ==="

if ! "$PROGRAMMER_CLI" -c port=SWD mode=UR -el "$EXTERNAL_LOADER" -d "$FIRMWARE" "$QSPI_ADDR" -rst; then
    echo "QSPI Flash failed."
    exit 1
fi

# Simulate pushd/popd with directory stack
pushd "ST/STM32F7508-Discovery/bootloader" > /dev/null

# Run bootloader flash script
if ! ./flash.sh; then
    echo "QSPI Flash failed in bootloader."
    popd > /dev/null
    exit 1
fi

popd > /dev/null

echo "QSPI Flash complete."
