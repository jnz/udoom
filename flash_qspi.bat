@echo off
set "Firmware=firmware.hex"
set "External_Loader=%STM32CLT_PATH%\STM32CubeProgrammer\bin\ExternalLoader\N25Q128A_STM32F7508-DISCO.stldr"

echo === Flash to ST-LINK QSPI Under Reset Mode ===

STM32_Programmer_CLI.exe -c port=SWD mode=UR -el "%External_Loader%" -d "%Firmware%" -rst

IF ERRORLEVEL 1 (
    echo QSPI Flash failed.
    exit /b 1
)

pushd ST\STM32F7508-Discovery\bootloader
call flash.bat
set "FLASH_RESULT=%ERRORLEVEL%"
popd

IF %FLASH_RESULT% NEQ 0 (
    echo QSPI Flash failed in bootloader.
    exit /b 1
)

echo QSPI Flash complete.

