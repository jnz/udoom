#include <stdint.h>
#include <stdio.h>
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

extern pixel_t* DG_ScreenBuffer; /* */

static void MPU_Config(void);
static void CPU_CACHE_Enable(void);
static void SystemClock_Config(void);

char SDPath[4];   /* SD logical drive path */
FATFS SDFatFS;    /* File system object for SD logical drive */

int main(void)
{
    MPU_Config();
    CPU_CACHE_Enable();
    HAL_Init();
    SystemClock_Config();

    // LEDs
    BSP_LED_Init(LED1);
    BSP_LED_Init(LED2);
    BSP_LED_On(LED1);

    // SDRAM
    BSP_SDRAM_Init();

    // Display
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, g_fblist[0]);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_SetLayerVisible(0, ENABLE);
    BSP_LCD_SetTransparency(0, 255);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
    BSP_LCD_SetBrightness(100); // set brightness to 100%

    memset((void *)g_fblist[0], 0, FRAMEBUFFER_HEIGHT * FRAMEBUFFER_WIDTH * BYTES_PER_PIXEL);
    memset((void *)g_fblist[1], 0, FRAMEBUFFER_HEIGHT * FRAMEBUFFER_WIDTH * BYTES_PER_PIXEL);

    int line = 0;
    BSP_LCD_DisplayStringAtLine(line++, "STM32F769I Doom by Jan Zwiener");
    BSP_LCD_DisplayStringAtLine(line++, "Loading .WAD file from SD card...");
    // Mount SD Card
    if (FATFS_LinkDriver(&SD_Driver, SDPath) != 0)
    {
        BSP_LCD_DisplayStringAtLine(line++, "Failed to load SD card driver");
        while (1) { HAL_Delay(1000); }
    }
    FRESULT fr = f_mount(&SDFatFS, (TCHAR const *)SDPath, 1);
    if (fr != FR_OK)
    {
        BSP_LCD_DisplayStringAtLine(line++, "Error: Failed to mount SD card.");
        while (1) { HAL_Delay(1000); }
    }
    // from now on fopen() and other stdio functions will work with the SD card

    BSP_LCD_DisplayStringAtLine(line++, "Loading Doom...");
    BSP_LED_Off(LED1); // MCU init complete

    /* Prepare main loop */
    char *argv[] = { "doom.exe" };
    doomgeneric_Create(sizeof(argv)/sizeof(argv[0]), argv);
    memset((void *)g_fblist[0], 0, FRAMEBUFFER_HEIGHT * FRAMEBUFFER_WIDTH * BYTES_PER_PIXEL);
    memset((void *)g_fblist[1], 0, FRAMEBUFFER_HEIGHT * FRAMEBUFFER_WIDTH * BYTES_PER_PIXEL);

    HAL_LTDC_ProgramLineEvent(&hltdc_discovery, 0);
    while (1)
    {
        // Prepare the framebuffer for drawing
        DG_ScreenBuffer = (pixel_t*)g_fblist[g_fbcur];
        doomgeneric_Tick();

        while (g_fbready)
        {
            HAL_Delay(0); // wait until frame swap
        }
    }

    return 0;
}

void DG_Init()
{
    DG_ScreenBuffer = (pixel_t*)g_fblist[g_fbcur];
}

void DG_DrawFrame()
{
    g_fbready = 1; // indicate that we can swap the frame
}

/** @brief  LTDC line event callback. Ready to draw the next frame.  */
void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *hltdc)
{
    if (g_fbready) // ready to swap frames?
    {
        // activate the next framebuffer
        LTDC_LAYER(hltdc, 0)->CFBAR = ((uint32_t)g_fblist[g_fbcur]);
        __HAL_LTDC_RELOAD_CONFIG(hltdc);

        g_fbcur = 1 - g_fbcur;
        g_fbready = 0;
        BSP_LED_Toggle(LED2); /* some developer feedback */
    }
    HAL_LTDC_ProgramLineEvent(hltdc, 0);
}

/**
  * @brief  CPU L1-Cache enable.
  * @param  None
  * @retval None
  */
static void CPU_CACHE_Enable(void)
{
  /* Enable I-Cache */
  SCB_EnableICache();

  /* Enable D-Cache */
  SCB_EnableDCache();
}

/* The SD card stuff apparently works better with a 200 MHz configuration */
static void SystemClock_Config(void)
{
    HAL_StatusTypeDef ret = HAL_OK;
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    /* Enable HSE Oscillator and activate PLL with HSE as source */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 400;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 8;
    RCC_OscInitStruct.PLL.PLLR = 7;

    ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if(ret != HAL_OK)
    {
        while(1) { ; }
    }

    ret = HAL_PWREx_EnableOverDrive();
    if(ret != HAL_OK)
    {
        while(1) { ; }
    }

    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6);
    if(ret != HAL_OK)
    {
        while(1) { ; }
    }
}

/**
  * @brief  Configure the MPU attributes
  * @param  None
  * @retval None
  */
/**
  * @brief  Configure the MPU attributes
  * @param  None
  * @retval None
  */
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct;

  /* Disable the MPU */
  HAL_MPU_Disable();

  /* Configure the MPU as Strongly ordered for not defined regions */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0x00;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Configure the MPU attributes as WT for SDRAM */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0xC0000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_32MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Configure the MPU attributes FMC control registers */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0xA0000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_8KB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER2;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enable the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

