@echo off

:: === Set Option Bytes ===
echo.
echo === Ensuring BOOT_ADD0 is set to internal flash 0x08000000 ===
echo Right shift target address by 14 bits i.e. divide by 16384
STM32_Programmer_CLI.exe -c port=SWD -ob BOOT_ADD0=0x2000

:: === Flash ===
echo.
echo === Flashing to STM32 via ST-LINK ===
STM32_Programmer_CLI.exe -c port=SWD mode=UR -d "bootloader.hex" -rst

IF ERRORLEVEL 1 (
    echo Flash failed.
    exit /b 1
)
