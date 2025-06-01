#!/bin/bash
set -e

FIRMWARE_BIN="firmware.bin"
FIRMWARE_HEX="firmware.hex"

QSPI_BASE="0x90000000"
QSPI_LOADER="N25Q128A_STM32F7508-DISCO.stldr"
QSPI_LOADER_PATH="ExternalLoader/${QSPI_LOADER}"

# Check if QSPI mode was requested via argument
explicit_qspi=0
if [ "$1" == "QSPI" ]; then
    explicit_qspi=1
fi

if arm-none-eabi-objdump -h "$FIRMWARE_ELF" | grep -q '90000000'; then
    echo "Detected external QSPI flash firmware. This requires STM32_Programmer_CLI."
    explicit_qspi=1
fi

if [ "$explicit_qspi" -eq 1 ]; then
    echo "QSPI firmware selected. Using external loader: $QSPI_LOADER"
    if [ ! -f "$QSPI_LOADER_PATH" ]; then
        echo "Error: Loader not found at $QSPI_LOADER_PATH"
        exit 1
    fi

    STM32_Programmer_CLI \
        -c port=SWD \
        -el "$QSPI_LOADER_PATH" \
        -d "$FIRMWARE_HEX" "$QSPI_BASE" \
        -rst
else
    if command -v STM32_Programmer_CLI &> /dev/null; then
        STM32_Programmer_CLI -c port=SWD -d "$FIRMWARE_HEX" -rst
    else
        echo "STM32_Programmer_CLI not found, trying st-flash instead..."
        echo "Using st-flash to internal flash at $FLASH_ADDR..."
        st-flash write "$FIRMWARE_BIN" "$FLASH_ADDR"
    fi
fi

echo "=== Flash complete ==="

