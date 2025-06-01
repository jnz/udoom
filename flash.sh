#!/bin/bash

FIRMWARE="firmware.bin"

echo
echo "=== Flash to ST-LINK via st-flash ==="

# Flash to address 0x08000000 (typischer Flash-Start für STM32)
st-flash write "$FIRMWARE" 0x8000000

# Fehlerbehandlung
if [ $? -ne 0 ]; then
    echo "Flash failed."
    exit 1
else
    echo "Flash successful."
fi

