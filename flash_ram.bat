@echo off
echo.
echo === Load to STM32N6570 SRAM and Run ===
STM32_Programmer_CLI.exe -c port=SWD mode=UR -d "firmware.elf" --start 0x24100400

IF ERRORLEVEL 1 (
    echo RAM load failed.
    exit /b 1
)
