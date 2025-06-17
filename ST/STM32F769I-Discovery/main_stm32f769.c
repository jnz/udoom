/*
             | |
    _   _  __| | ___   ___  _ __ ___
   | | | |/ _` |/ _ \ / _ \| '_ ` _ \
   | |_| | (_| | (_) | (_) | | | | | |
    \__,_|\__,_|\___/ \___/|_| |_| |_|


   Doom for the STM32F769 microcontroller
   */

/******************************************************************************
 * INCLUDE FILES
 ******************************************************************************/

#include <stdio.h>
#include <stdarg.h>
// Board specific includes
#include "stm32f7xx.h"
#include "stm32f769i_discovery.h"
#include "stm32f769i_discovery_lcd.h"
#include "stm32f769i_discovery_sdram.h"
#include "main_stm32f7xx.h"
#include "memusage.h"
/* <mass storage> */
#include "ff_gen_drv.h"
#include "sd_diskio.h"
/* </mass storage> */

// Doom includes
#include "doomkeys.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

// Framebuffer
#define FRAMEBUF_PIXELS    (OTM8009A_800X480_WIDTH * OTM8009A_800X480_HEIGHT)

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/******************************************************************************
 * GLOBAL DATA DEFINITIONS
 ******************************************************************************/

// Double Buffering
// Discovery Board has 2 framebuffers in SDRAM for the display in a high
// resolution with 32bit color depth.
__attribute__((section(".framebuffer1"))) uint32_t framebuffer1[FRAMEBUF_PIXELS];
__attribute__((section(".framebuffer2"))) uint32_t framebuffer2[FRAMEBUF_PIXELS];
extern LTDC_HandleTypeDef hltdc_discovery;
extern uint8_t* STM32_ScreenBuffer; // 320x200 buffer for doom (8bpp)

// UART
UART_HandleTypeDef  huart1;
static uint8_t      g_uart_rx_byte; // only modified in interrupt handler

char sdpath[4]; // SD logical drive path
FATFS sdfatfs;  // File system object for SD logical drive

/******************************************************************************
 * LOCAL DATA DEFINITIONS
 ******************************************************************************/


/******************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 ******************************************************************************/

// MCU config
static void MPU_Config(void);
static void CPU_CACHE_Enable(void);
static void SystemClock_Config(void);

// UART
static void MX_USART1_UART_Init(void);

/******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

/******************************************************************************
 * FUNCTION BODIES
 ******************************************************************************/

void I_BoardInit(void)
{
    MPU_Config();
    CPU_CACHE_Enable();
    HAL_Init();
    SystemClock_Config();
    stack_fill_with_magic();
    enable_dwt_cycle_counter();

    // LEDs
    BSP_LED_Init(LED1);
    BSP_LED_Init(LED2);
    BSP_LED_On(LED1);

    // SDRAM
    BSP_SDRAM_Init();

    // UART
    MX_USART1_UART_Init();

    // Display
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, (uint32_t)framebuffer1);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_SetLayerVisible(0, ENABLE);
    BSP_LCD_SetTransparency(0, 255);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
    BSP_LCD_SetBrightness(100); // brightness in %
    I_FramebufferClearAll();

    HAL_LTDC_ProgramLineEvent(&hltdc_discovery, 0);

    // Mount SD Card
    int line = 0;
    BSP_LCD_DisplayStringAtLine(line++, "STM32F769I Doom (jan@zwiener.org)");
    BSP_LCD_DisplayStringAtLine(line++, "Mounting SD Card...");
    if (FATFS_LinkDriver(&SD_Driver, sdpath) != 0)
    {
        I_Error("Failed to load SD card driver");
    }
    FRESULT fr = f_mount(&sdfatfs, (TCHAR const *)sdpath, 1);
    if (fr != FR_OK)
    {
        I_Error("Error: Failed to mount SD card. (%d)", fr);
    }
    // from now on fopen() and other stdio functions will work with the SD card
    BSP_LED_Off(LED1); // MCU init complete

}

void I_FramebufferClearAll(void)
{
    memset((void *)framebuffer1, 0x00, sizeof(framebuffer1));
    memset((void *)framebuffer2, 0x00, sizeof(framebuffer2));
}

uint32_t* I_FramebufferGet(int index)
{
    switch (index)
    {
        case 0: return framebuffer1;
        case 1: return framebuffer2;
        default:
            I_Error("I_FramebufferGet: Invalid index %d", index);
            return NULL;
    }
}

/** @brief  CPU L1-Cache enable.  */
static void CPU_CACHE_Enable(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
}

/* The SD card stuff should run with a 200 MHz MCU configuration according to
 * the readme.txt of ST */
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

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);

    HAL_UART_Receive_IT(&huart1, (uint8_t *)&g_uart_rx_byte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        I_StdinByteRecv(g_uart_rx_byte);

        // Restart the UART receive interrupt
        HAL_UART_Receive_IT(huart, (uint8_t *)&g_uart_rx_byte, 1);
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(uartHandle->Instance == USART1)
    {
        /* USART1 clock enable */
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /**USART1 GPIO Configuration
          PA9  ------> USART1_TX
          PA10 ------> USART1_RX
          */
        GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* USART1 interrupt Init */
        HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    }
}

/* syscalls.c will call this function for stdout and stderr,
 * redirects to USART1. */
int __io_putchar(int ch)
{
    /* DMA is not used to simplify things */

    if (ch == '\n') /* play nice with putty */
    {
        char r = '\r';
        HAL_UART_Transmit(&huart1, (const uint8_t*)&r, 1, 1);
    }
    HAL_UART_Transmit(&huart1, (const uint8_t*)&ch, 1, 1);
    return ch;
}

void I_ErrorDisplay(const char* msg)
{
    // Try to emit the error to the display. Go to framebuffer 0.
    // then draw the error string to the top left corner
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_SetBackColor(LCD_COLOR_RED);
    __HAL_LTDC_DISABLE_IT(&hltdc_discovery, LTDC_IT_LI);
    LTDC_LAYER(&hltdc_discovery, 0)->CFBAR = ((uint32_t)framebuffer1);
    __DSB();
    __HAL_LTDC_RELOAD_CONFIG(&hltdc_discovery);
    BSP_LCD_DisplayStringAtLine(0, (uint8_t*)msg);
}

