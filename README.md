µDoom
=====

              | |
     _   _  __| | ___   ___  _ __ ___
    | | | |/ _` |/ _ \ / _ \| '_ ` _ \
    | |_| | (_| | (_) | (_) | | | | | |
     \__,_|\__,_|\___/ \___/|_| |_| |_|

   Doom for the STM32F7 microcontroller

Microcontroller Doom — runs classic DOOM on the STM32F769I-Discovery board.


Toolchain Setup
---------------

Download and extract the ARM GCC toolchain (tested with version 13.3.rel1):

<https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads>

Extract to a folder.

Edit `config.mk` and point `TOOLCHAIN_ROOT` to that folder (ensure it ends with a slash!).
On Linux e.g.:

    TOOLCHAIN_ROOT= /home/user/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi/bin/

On Windows e.g.:

    TOOLCHAIN_ROOT=C:/Tools/arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi/bin/

Then run make.

Flashing the Firmware for the STM32F769I-Discovery Board:

    make flash

The STM32F769I-Discovery version requires a FAT32 SD card with a .wad file
(e.g. doom1.wad) in its root directory. The STM32F7508 version
has a huge flash memory and the `doom1.wad` (Shareware version) is
embedded in the firmware, so no SD card is required.

To program the STM32F7508:

    make clean
    make -j4 BOARD=STM32F7508_DK
    ./flash_qspi.bat

Then program the bootloader into the internal flash:

    cd ST/STM32F7508-Discovery/bootloader
    ./flash.bat


Flash Tool
----------

To write the firmware to the target device on Windows, download
`STM32_Programmer_CLI.exe` from this package from ST:

    https://www.st.com/en/development-tools/stm32cubeprog.html

Or on Linux:

    sudo apt install stlink-tools

Debugger Configuration for VS Code
----------------------------------

On Linux install OpenOCD:

    sudo apt install openocd

On Windows download OpenOCD (tested with version 0.12.0-6):

    https://github.com/xpack-dev-tools/openocd-xpack/releases

Extract to a folder.

Install the Visual Studio Code plugin Cortex-Debug.

Add the following to your VS Code settings.json (adapt paths as needed):

    "cortex-debug.openocdPath.windows": "C:/Tools/xpack-openocd-0.12.0-6/bin/openocd.exe",
    "cortex-debug.gdbPath.windows": "C:/Tools/arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-gdb.exe"

Debugging in VS Code should now work.

Manual Debugging
----------------

(Path examples for Windows, adjust for Linux)

Run OpenOCD with the target board connected:

    C:\Tools\xpack-openocd-0.12.0-6\bin\openocd -f interface/stlink.cfg -f target/stm32f7x.cfg

Run GDB:

    C:\Tools\arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi\bin\arm-none-eabi-gdb firmware.elf

In GDB:

    target remote localhost:3333
    monitor reset halt
    load
    break main
    continue

For an additional QSPI firmware in GDB:

    file bootloader.elf

Add main firmware symbols:

    add-symbol-file firmware.elf 0x90000000

Or relative to the bootloader directory:

    add-symbol-file ../../../firmware.elf 0x90000000

    info address Reset_Handler

You see something like:

    Symbol "Reset_Handler" is at 0x08000100 in bootloader.elf
    Symbol "Reset_Handler" is at 0x9003004d in firmware.elf

    break *0x9003004d

(or whatever the real address is)

Debug in GDB with

    n (next)
    s (step)
    si (assembler instruction step)
    finish (to end of function)

With ST-LINK GDB server:

    target remote localhost:XXXX # default port is 6123
    monitor reset
    monitor halt

DMA2D
-----

Doom renders internally into `I_VideoBuffer`, a 320×200 8-bit (L8) framebuffer.
DMA2D is used to convert this to a 32-bit ARGB8888 image for the display.

    ⚠️ Note: DMA2D does not support scaling. Scaling to 640×400 must be done in software.

The color palette is defined in `doomgeneric/stm32f7/i_video.c`:

    static uint32_t dma2d_clut[256];

To load the CLUT:

    DMA2D_CLUTCfgTypeDef clut_cfg = {
        .pCLUT = dma2d_clut,
        .CLUTColorMode = DMA2D_CCM_ARGB8888,
        .Size = 255
    };
    SCB_CleanDCache_by_Addr((uint32_t*)dma2d_clut, sizeof(dma2d_clut));
    HAL_DMA2D_CLUTLoad(&hdma2d, clut_cfg, 1);

Then convert and copy the frame:

    SCB_CleanDCache_by_Addr(src, width * height);
    HAL_DMA2D_Start(&hdma2d,
                    (uint32_t)src, (uint32_t)dst,
                    width, height);

src is the scaled 640×400 8-bit buffer.
dst is the ARGB framebuffer in SDRAM.

Display Framebuffer
-------------------

The LCD is initialized with the usual boilerplate code from the STM32F7 BSP:

    BSP_LCD_Init();

Double buffering is used to avoid tearing. Two framebuffers are placed in SDRAM (after enabling it with `BSP_SDRAM_Init()`):

    static uint32_t g_fblist[2];
    g_fblist[0] = 0xC0000000;
    g_fblist[1] = g_fblist[0] + 800 * 480 * 4; // 4 bytes per pixel

Set the initial framebuffer:

    BSP_LCD_LayerDefaultInit(0, g_fblist[0]);

To sync buffer swaps with vertical refresh, use a line event interrupt:

    HAL_LTDC_ProgramLineEvent(&hltdc_discovery, 0);

Handle it like this:

    void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *hltdc)
    {
        LTDC_LAYER(hltdc, 0)->CFBAR = g_fblist[g_fbcur];
        __DSB();
        __HAL_LTDC_RELOAD_CONFIG(hltdc);

        g_fbcur = 1 - g_fbcur;
        HAL_LTDC_ProgramLineEvent(hltdc, 0); // request next line event
    }

While one buffer is displayed, Doom draws into the other.

Zone Alloc and Memory Layout
----------------------------

Doom uses its own memory allocator instead of malloc(). It needs about 6 MB of RAM.

This is allocated from SDRAM, after the two framebuffers:

    #define ZONE_BASE_ADDR (0xC0000000 + 800 * 480 * 4 * 2)

Doom calls this at startup:

    uint8_t *I_ZoneBase(int *size)
    {
        *size = NUMBER_OF_BYTES_FOR_Z_MALLOC;
        return (uint8_t*)ZONE_BASE_ADDR;
    }

Make sure this block is large enough and does not overlap with any other DMA buffers.

Reading .WAD Files from SD Card
-------------------------------

A bunch of boilerplate code is needed to read data from SD card using FatFS.
The code is located in `src/storage/*`.

To mount the SD card, first initialize the SD driver and link it to a path:

    char sdpath[4];
    FATFS sdfatfs;
    if (FATFS_LinkDriver(&SD_Driver, sdpath) != 0)
    {
        BSP_LCD_DisplayStringAtLine(line++, "Failed to load SD card driver");
        /* error handling */
    }
    FRESULT fr = f_mount(&sdfatfs, (TCHAR const *)sdpath, 1);
    if (fr != FR_OK)
    {
        BSP_LCD_DisplayStringAtLine(line++, "Error: Failed to mount SD card.");
        /* error handling */
    }

As Doom is has `fopen(...)` and `fread(...)` calls all over the place, the
simplest way is to link the stdio library functions to FatFs in the
`syscalls.c` file by overwriting `_open`, `_lseek`, `_read`, `_write`, and `_close`.
E.g. for  `_open`, this will link fopen to FatFs's `f_open` function:

    int _open(const char *path, int flags, ...)
    {
        BYTE fatfs_mode = 0;

        if ((flags & O_RDWR) == O_RDWR)
            fatfs_mode = FA_READ | FA_WRITE;
        else if (flags & O_WRONLY)
            fatfs_mode = FA_WRITE;
        else
            fatfs_mode = FA_READ;

    #if _FS_READONLY == 0
        if (flags & O_CREAT)
            fatfs_mode |= FA_OPEN_ALWAYS;
    #endif

        for (int fn = 0; fn < MAX_FILES; fn++)
        {
            if (g_files[fn].obj.fs == NULL)
            {
                FRESULT fr = f_open(&g_files[fn], path, fatfs_mode);
                if (fr == FR_OK)
                {
                    return fn + RESERVED_FILE_HANDLES;
                }
                errno = EIO;
                return -1;
            }
        }
        errno = EMFILE;
        return -1;
    }

with a max. pool of FatFs file handles (`FIL`):

    static FIL g_files[MAX_FILES];

Bootloader for STM32F7508
-------------------------

The STM32F7508 has 16MB of QSPI flash memory at 0x90000000.
Now the cumbersome part is that this requires a custom bootloader
to initialize the QSPI hardware and then jump to the application code.

The custom bootloader can be placed in the 64KB internal flash memory at
0x08000000. There is a bootloader in the directory `ST/STM32F7508-Discovery/bootloader`.
This bootloader initializes the QSPI flash and then jumps to the application.
Use `make` to build the bootloader. Then run `flash.bat` in the directory
`ST/STM32F7508-Discovery/bootloader` to load the bootloader. If you don't want
to compile the bootloader, there is a precompiled binary in the same directory
`bootloader_precompiled.hex`.
The `flash.bat` script will also setup the `BOOT_ADD0` address for the board:

    echo === Ensuring BOOT_ADD0 is set to internal flash (0x08000000) ===
    echo Right shift target address by 14 bits (i.e. divide by 16384)
    STM32_Programmer_CLI.exe -c port=SWD -ob BOOT_ADD0=0x2000

So `BOOT_ADD0` is set to 0x2000 (which means 0x08000000), which is the internal
flash memory.

