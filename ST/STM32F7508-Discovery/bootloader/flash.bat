@echo off

set "Firmware=bootloader.elf"

:: Convert ELF to HEX
echo.
echo === Generating HEX file ===
arm-none-eabi-objcopy -O ihex %Firmware% bootloader.hex

:: === Set Option Bytes ===
echo.
echo === Ensuring BOOT_ADD0 is set to internal flash (0x08000000) ===
echo Right shift target address by 14 bits (i.e. divide by 16384)
STM32_Programmer_CLI.exe -c port=SWD -ob BOOT_ADD0=0x2000

:: === Flash ===
echo.
echo === Flashing to STM32 via ST-LINK ===
STM32_Programmer_CLI.exe -c port=SWD -d bootloader.hex -v -rst

IF ERRORLEVEL 1 (
    echo Flash failed.
    exit /b 1
)
