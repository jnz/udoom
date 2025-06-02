@echo off
set "Firmware=firmware.hex"
set "QSPI_Address=0x90000000"
set "External_Loader=%STM32CLT_PATH%\STM32CubeProgrammer\bin\ExternalLoader\N25Q128A_STM32F7508-DISCO.stldr"

echo === Flash to ST-LINK QSPI (Under Reset Mode) ===

REM Erst versuchen mit Under Reset Mode
STM32_Programmer_CLI.exe -c port=SWD mode=UR -el "%External_Loader%" -d "%Firmware%" %QSPI_Address% -v -rst

IF ERRORLEVEL 1 (
    echo Retry with slower frequency...
    STM32_Programmer_CLI.exe -c port=SWD mode=UR freq=4000 -el "%External_Loader%" -d "%Firmware%" %QSPI_Address% -v -rst
    
    IF ERRORLEVEL 1 (
        echo QSPI Flash failed.
        exit /b 1
    ) ELSE (
        echo QSPI Flash successful with slower frequency!
    )
) ELSE (
    echo QSPI Flash successful!
)
