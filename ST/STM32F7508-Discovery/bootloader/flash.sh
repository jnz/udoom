#!/bin/bash
set -e

STM32_Programmer_CLI.exe -c port=SWD -ob BOOT_ADD0=0x2000

STM32_Programmer_CLI.exe -c port=SWD -d bootloader_precompiled.hex -v -rst

echo "=== Flash complete ==="

