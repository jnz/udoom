#include <stdint.h>
#include "stm32f7xx.h"
#include "stm32f769i_discovery.h"
#include "stm32f769i_discovery_lcd.h"
#include "stm32f769i_discovery_sdram.h"
/* <mass storage> */
#include "ff_gen_drv.h"
#include "sd_diskio.h"
/* </mass storage> */
#include "doomgeneric.h"

#define FRAMEBUFFER_WIDTH  800
#define FRAMEBUFFER_HEIGHT 480
#define BYTES_PER_PIXEL    4

// Double Buffering
#define FB0_ADDR LCD_FB_START_ADDRESS
#define FB1_ADDR ((uint32_t)(FB0_ADDR + FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * BYTES_PER_PIXEL))
// list with framebuffer addresses
static const uint32_t g_fblist[] = { FB0_ADDR, FB1_ADDR };
static int g_fbcur = 1; // start in invisible buffer
static int g_fbready = 0; // safe to swap?
extern LTDC_HandleTypeDef hltdc_discovery;

extern void SystemClock_Config(void);
extern void CPU_CACHE_Enable(void);

extern pixel_t* DG_ScreenBuffer;

int main(void)
{
    CPU_CACHE_Enable();
    HAL_Init();
    SystemClock_Config();

    BSP_LED_Init(LED2);
    BSP_SDRAM_Init();

    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, g_fblist[0]);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_SetLayerVisible(0, ENABLE);
    BSP_LCD_SetTransparency(0, 255);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_SetBackColor(LCD_COLOR_BLACK);

    HAL_LTDC_ProgramLineEvent(&hltdc_discovery, 0);

    memset((void *)g_fblist[0], 0, FRAMEBUFFER_HEIGHT * FRAMEBUFFER_WIDTH * BYTES_PER_PIXEL);
    memset((void *)g_fblist[1], 0, FRAMEBUFFER_HEIGHT * FRAMEBUFFER_WIDTH * BYTES_PER_PIXEL);

    if (BSP_SD_Init() != MSD_OK)
    {
        BSP_LCD_DisplayStringAtLine(1, (uint8_t *)"SD init failed!");
        while (1) {}
    }

    /* Prepare doomgeneric */
    char *argv[] = { "doom.exe" };
    doomgeneric_Create(1, argv);

    while (1)
    {
        // Prepare the framebuffer for drawing
        uint32_t* drawFB = (uint32_t*)g_fblist[g_fbcur];
        DG_ScreenBuffer = (pixel_t*)drawFB;

        doomgeneric_Tick();

        // SCB_CleanDCache_by_Addr((uint32_t*)drawFB, FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * BYTES_PER_PIXEL);
        while (g_fbready)
        {
            HAL_Delay(0); // wait until frame swap
        }
    }

    return 0;
}

void DG_DrawFrame()
{
    g_fbready = 1; // indicate that we can swap the frame
}


void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *hltdc)
{
    if (g_fbready) // ready to swap frames?
    {
        LTDC_LAYER(hltdc, 0)->CFBAR = ((uint32_t)g_fblist[g_fbcur]);
        __HAL_LTDC_RELOAD_CONFIG(hltdc);

        g_fbcur = 1 - g_fbcur;
        g_fbready = 0;
        BSP_LED_Toggle(LED2);
    }
    HAL_LTDC_ProgramLineEvent(hltdc, 0);
}

#if 0
static int sdcard_test_wad(void)
{
    FATFS fs;
    FIL fil;
    UINT br;
    uint8_t buf[4];
    char path[64];
    int success = -1;

    if (FATFS_LinkDriver(&SD_Driver, path) != 0)
    {
        BSP_LCD_DisplayStringAtLine(1, (uint8_t *)"Link driver failed!");
        return success;
    }

    if (f_mount(&fs, "", 1) != FR_OK)
    {
        BSP_LCD_DisplayStringAtLine(1, (uint8_t *)"Mount failed!");
        return success;
    }

    BSP_LCD_DisplayStringAtLine(1, (uint8_t *)"Mounted. Reading...");

    if (f_open(&fil, "DOOM1.WAD", FA_READ) != FR_OK)
    {
        BSP_LCD_DisplayStringAtLine(2, (uint8_t *)"DOOM1.WAD not found on root of SD card (FAT32).");
        return success;
    }

    if (f_read(&fil, buf, sizeof(buf), &br) == FR_OK && br > 0)
    {
        if (buf[0] == 'I' && buf[1] == 'W' && buf[2] == 'A' && buf[3] == 'D')
        {
            BSP_LCD_DisplayStringAtLine(2, (uint8_t *)"Valid DOOM1.WAD found!");
            success = 0;
        }
        else
        {
            BSP_LCD_DisplayStringAtLine(2, (uint8_t *)"DOOM1.WAD not valid.");
        }
    }
    else
    {
        BSP_LCD_DisplayStringAtLine(3, (uint8_t *)"Read failed.");
    }

    f_close(&fil);
    return success;
}
#endif

