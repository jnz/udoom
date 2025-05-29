µDoom
=====

Microcontroller Doom — runs classic DOOM on the STM32F769I-Discovery board.

Toolchain Setup
---------------

Download and extract the ARM GCC toolchain (tested with version 13.3.rel1):

<https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads>

Extract to a folder.

Edit `config.mk` and point `TOOLCHAIN_ROOT` to that folder (ensure it ends with a slash):

    TOOLCHAIN_ROOT=C:/Tools/arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi/bin/

Then run make.

Debugger Configuration for VS Code on Windows
---------------------------------------------

Download OpenOCD (tested with version 0.12.0-6):

    https://github.com/xpack-dev-tools/openocd-xpack/releases

Extract to a folder.

Install the Visual Studio Code plugin Cortex-Debug.

Add the following to your VS Code settings.json (adapt paths as needed):

    "cortex-debug.openocdPath.windows": "C:/Tools/xpack-openocd-0.12.0-6/bin/openocd.exe",
    "cortex-debug.gdbPath.windows": "C:/Tools/arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-gdb.exe"

Debugging in VS Code should now work.

Manual Debugging
----------------

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

DMA2D
-----

Doom renders internally into I_VideoBuffer, a 320×200 8-bit (L8) framebuffer.
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

