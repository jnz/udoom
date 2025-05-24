µDoom
=====

Microcontroller Doom.


Toolchain Setup
---------------

Download and extract the ARM GCC toolchain (tested with 13.3.rel1)

    https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads

Extract to some folder.

Edit `config.mk` and point to the toolchain folder (don't forget the `/` at the end):

    TOOLCHAIN_ROOT=C:/Tools/arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi/bin/

Running `make` should work now.

Debugger Configuration for VS Code on Windows
---------------------------------------------

Download OpenOCD (tested with 0.12.0-6):

    https://github.com/xpack-dev-tools/openocd-xpack/releases

Extract to some folder.

Install the Visual Studio `Cortex Debugger` plugin.

Add to your `settings.json` in VS Code (example path):

    "cortex-debug.openocdPath.windows": "C:/Tools/xpack-openocd-0.12.0-6/bin/openocd.exe",
    "cortex-debug.gdbPath.windows": "C:/Tools/arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-gdb.exe",

Debugging in Visual Studio Code under Windows should work now.

Manually Debug
--------------

Run OpenOCD with target board connected:

    C:\Tools\xpack-openocd-0.12.0-6\bin\openocd -f interface/stlink.cfg -f target/stm32f7x.cfg

Run gdb:

    C:\Tools\arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi\bin\arm-none-eabi-gdb firmware.elf

Within GDB:

    target remote localhost:3333
    monitor reset halt
    load
    break main
    continue
