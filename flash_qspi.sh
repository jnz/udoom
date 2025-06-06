#!/bin/bash
set -e

FIRMWARE="firmware.hex"
QSPI_ADDR="0x90000000"

# STM32CubeProgrammer root install path (can override via env var)
STM32CLT_PATH="${STM32CLT_PATH:-/opt/stm32cube/stm32cubeprogrammer}"
PROGRAMMER_CLI="$STM32CLT_PATH/bin/STM32_Programmer_CLI"
EXTERNAL_LOADER="$STM32CLT_PATH/bin/ExternalLoader/N25Q128A_STM32F7508-DISCO.stldr"

# Check toolchain components exist
if [ ! -x "$PROGRAMMER_CLI" ]; then
    echo "Error: STM32_Programmer_CLI not found at $PROGRAMMER_CLI"
    exit 1
fi

if [ ! -f "$EXTERNAL_LOADER" ]; then
    echo "Error: External loader not found at $EXTERNAL_LOADER"
    exit 1
fi

echo "=== Flash to ST-LINK QSPI (Under Reset Mode) ==="

# First attempt
if "$PROGRAMMER_CLI" -c port=SWD mode=UR -el "$EXTERNAL_LOADER" -d "$FIRMWARE" "$QSPI_ADDR" -rst; then
    echo "QSPI Flash successful!"
else
    echo "Retry with slower frequency..."
    if "$PROGRAMMER_CLI" -c port=SWD mode=UR freq=4000 -el "$EXTERNAL_LOADER" -d "$FIRMWARE" "$QSPI_ADDR" -rst; then
        echo "QSPI Flash successful with slower frequency!"
    else
        echo "QSPI Flash failed."
        exit 1
    fi
fi
