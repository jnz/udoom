@echo off

set "Firmware=firmware.hex"

REM :: === List Devices ===
REM echo.
REM echo === Available ST-LINKs ===
REM STM32_Programmer_CLI.exe -l

:: === Flash ===
echo === Flash to ST-LINK QSPI ===
REM STM32_Programmer_CLI.exe -c port=SWD -el C:\ST\STM32CubeCLT_1.18.0\STM32CubeProgrammer\bin\ExternalLoader\N25Q128A_STM32F7508-DISCO.stldr -d firmware.hex 0x90000000 -v -rst
STM32_Programmer_CLI.exe -c port=SWD -el %STM32CLT_PATH%\STM32CubeProgrammer\bin\ExternalLoader\N25Q128A_STM32F7508-DISCO.stldr -d firmware.hex 0x90000000 -v -rst

IF ERRORLEVEL 1 (
    echo Flash failed.
    exit /b 1
)

