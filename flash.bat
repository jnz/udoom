@echo off

REM :: === List Devices ===
REM echo.
REM echo === Available ST-LINKs ===
REM STM32_Programmer_CLI.exe -l

:: === Flash ===
echo.
echo === Flash to ST-LINK ===
STM32_Programmer_CLI.exe -c port=SWD mode=UR -d "firmware.hex" -rst

IF ERRORLEVEL 1 (
    echo Flash failed.
    exit /b 1
)

