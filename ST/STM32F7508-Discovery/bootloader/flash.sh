#!/bin/bash
set -e

STM32_Programmer_CLI -c port=SWD -ob BOOT_ADD0=0x2000

STM32_Programmer_CLI -c port=SWD mode=UR -d bootloader.hex -v -rst

echo "=== Flash complete ==="

