/*
   | |
   _   _  __| | ___   ___  _ __ ___
   | | | |/ _` |/ _ \ / _ \| '_ ` _ \
   | |_| | (_| | (_) | (_) | | | | | |
   \__,_|\__,_|\___/ \___/|_| |_| |_|

   Doom for the STM32F7 microcontroller
   */

/******************************************************************************
 * INCLUDE FILES
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
// Board specific includes
#include "stm32f7xx.h"
#include "stm32f769i_discovery.h"
#include "stm32f769i_discovery_lcd.h"
#include "stm32f769i_discovery_sdram.h"
/* <mass storage> */
#include "ff_gen_drv.h"
#include "sd_diskio.h"
/* </mass storage> */
#include "doomgeneric.h"
#include "doomkeys.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

// Framebuffer
#define BYTES_PER_PIXEL  4
#define SDRAM_BASE       LCD_FB_START_ADDRESS
#define SDRAM_SIZE       (8 * 1024 * 1024)  // MB
#define SDRAM_END        (SDRAM_BASE + SDRAM_SIZE)
#define FB_SIZE_BYTES    (BSP_LCD_GetXSize() * BSP_LCD_GetYSize() * BYTES_PER_PIXEL)

// UART
#define UART_TX_BUF_SIZE     256
#define UART_RX_BUF_SIZE     2   // must be power of 2
#define USART_TX_Pin         GPIO_PIN_9
#define USART_TX_GPIO_Port   GPIOA
#define USART_RX_Pin         GPIO_PIN_10
#define USART_RX_GPIO_Port   GPIOA
#define UART_KEY_HOLD_MS     100 // mark a key as released after xxx ms over uart

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/******************************************************************************
 * GLOBAL DATA DEFINITIONS
 ******************************************************************************/

// Double Buffering
extern LTDC_HandleTypeDef hltdc_discovery; // display handle
extern pixel_t* DG_ScreenBuffer; // buffer for doom to draw to

// UART
UART_HandleTypeDef  huart1;
DMA_HandleTypeDef   hdma_usart1_tx;

/******************************************************************************
 * LOCAL DATA DEFINITIONS
 ******************************************************************************/

// Double Buffering
static uint32_t g_fblist[2];
static uint32_t g_vsync_count;
static bool g_double_buffer_enabled = false;

// Modified from interrupt handler and main code path:
volatile static int g_fbcur = 1; // index into g_fblist, start in invisible buffer
volatile static int g_fbready = 0; // safe to swap buffers?

// UART
static uint8_t g_uart_rx_byte;
static uint8_t g_uart_rx_buf[UART_RX_BUF_SIZE];
static int g_uart_rx_buf_size; // number of bytes in g_uart_rx_buf

/******************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 ******************************************************************************/

// MCU config
static void MPU_Config(void);
static void CPU_CACHE_Enable(void);
static void SystemClock_Config(void);
// Framebuffer
static void Framebuffer_Clear(void);
// UART
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
// Profiler with cycle counter
static void enable_dwt_cycle_counter(void);

/******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

// HAL Delay with power saving
void HAL_Delay_WFI(uint32_t Delay);
// Doom error functions
void I_Error(char *error, ...);
void I_DoubleBufferEnable(int enable);

/******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

int main(void)
{
    MPU_Config();
    CPU_CACHE_Enable();
    HAL_Init();
    SystemClock_Config();
    enable_dwt_cycle_counter();

    // LEDs
    BSP_LED_Init(LED1);
    BSP_LED_Init(LED2);
    BSP_LED_On(LED1);

    // UART
    MX_DMA_Init();
    MX_USART1_UART_Init();
    printf("STM32F769I Discovery Doom\n"); // early sign of life
    printf("Core frequency: %lu MHz\n", HAL_RCC_GetHCLKFreq() / 1000000);

    // SDRAM
    BSP_SDRAM_Init();

    // Display
    g_fblist[0] = LCD_FB_START_ADDRESS;
    g_fblist[1] = g_fblist[0] + FB_SIZE_BYTES;
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, g_fblist[0]);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_SetLayerVisible(0, ENABLE);
    BSP_LCD_SetTransparency(0, 255);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
    BSP_LCD_SetBrightness(100); // set brightness to 100%
    Framebuffer_Clear();

    // Initial hello
    int line = 0;
    BSP_LCD_DisplayStringAtLine(line++, "STM32F769I Doom by Jan Zwiener");
    BSP_LCD_DisplayStringAtLine(line++, "Loading .WAD file from SD card...");
    // Mount SD Card
    char sdpath[4]; // SD logical drive path
    FATFS sdfatfs;  // File system object for SD logical drive
    if (FATFS_LinkDriver(&SD_Driver, sdpath) != 0)
    {
        BSP_LCD_DisplayStringAtLine(line++, "Failed to load SD card driver");
        I_Error("Failed to load SD card driver");
    }
    FRESULT fr = f_mount(&sdfatfs, (TCHAR const *)sdpath, 1);
    if (fr != FR_OK)
    {
        BSP_LCD_DisplayStringAtLine(line++, "Error: Failed to mount SD card.");
        I_Error("Error: Failed to mount SD card. (%d)", fr);
    }
    // from now on fopen() and other stdio functions will work with the SD card
    BSP_LED_Off(LED1); // MCU init complete

    // Prepare main loop
    char *argv[] = { "doom.exe" };
    doomgeneric_Create(sizeof(argv)/sizeof(argv[0]), argv);
    Framebuffer_Clear();

    I_DoubleBufferEnable(1);
    HAL_LTDC_ProgramLineEvent(&hltdc_discovery, 0);
    int fpscounter = 0;
    uint32_t nextfpsupdate = HAL_GetTick() + 1000;
    uint32_t cyclecount = 0; // cycles used for Doom (reset every second)
    while (1) /* main loop */
    {
        const uint32_t framestart = DWT->CYCCNT; // CPU usage helper

        // Prepare the framebuffer for drawing
        DG_ScreenBuffer = (pixel_t*)g_fblist[g_fbcur];
        doomgeneric_Tick();
        fpscounter++;

        cyclecount += DWT->CYCCNT - framestart; // count cycles used for this frame
        if (HAL_GetTick() > nextfpsupdate)
        {
            // ratio: CPU cycles spent on Doom vs total CPU cycles in 1 second
            const float cpuload = (float)cyclecount / HAL_RCC_GetHCLKFreq();
            printf("\rFPS %2i CPU%3u%% VSYNC%3u Hz",
                   fpscounter, (int)(cpuload * 100), g_vsync_count);
            cyclecount = 0;
            fpscounter = 0;
            g_vsync_count = 0;
            nextfpsupdate += 1000;
        }

        /* Wait for the display ISR to consume g_fbready */
        while (g_fbready)
        {
            __WFI(); // display VSYNC will generate an interrupt
        }
    }

    return 0;
}

static void Framebuffer_Clear(void)
{
    memset((void *)g_fblist[0], 0, FB_SIZE_BYTES); // clear first framebuffer
    memset((void *)g_fblist[1], 0, FB_SIZE_BYTES); // clear second framebuffer
}

void DG_Init() // called by Doom during init
{
    // give Doom something to draw to
    DG_ScreenBuffer = (pixel_t*)g_fblist[g_fbcur];
}

void DG_DrawFrame() // called by Doom at the end of a frame
{
    g_fbready = 1; // indicate that we can swap the frame
}

/** @brief LTDC line event callback. Ready to draw the next frame.  */
void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *hltdc)
{
    if (g_fbready) // ready to swap frames?
    {
        // activate the next framebuffer
        LTDC_LAYER(hltdc, 0)->CFBAR = ((uint32_t)g_fblist[g_fbcur]);
        __DSB();
        __HAL_LTDC_RELOAD_CONFIG(hltdc);

        if (g_double_buffer_enabled)
        {
            g_fbcur = 1 - g_fbcur;
        }
        g_fbready = 0;
        BSP_LED_Toggle(LED2); /* some developer feedback */
    }
    HAL_LTDC_ProgramLineEvent(hltdc, 0); // setup next VSYNC callback
    g_vsync_count++;
}

/** @brief Give the address and size of the zone memory.  */
uint8_t *I_ZoneBase (int *size)
{
    // Improvement: handle this via linker script
    uint32_t zonemem = g_fblist[1] + FB_SIZE_BYTES;
    printf("zonemem address: %p\n", (void*)zonemem);
    *size = SDRAM_END - zonemem;
    return (uint8_t*) zonemem;
}

void I_Error(char *error, ...)
{
    static bool already_quitting = false;
    char msgbuf[256];
    va_list argptr;

    if (already_quitting)
    {
        return;
    }
    already_quitting = true;

    va_start(argptr, error);
    vsnprintf(msgbuf, sizeof(msgbuf), error, argptr);
    va_end(argptr);

    // Try to emit the error to the display. Go to framebuffer 0.
    // then draw the error string to the top left corner
    __HAL_LTDC_DISABLE_IT(&hltdc_discovery, LTDC_IT_LI);
    g_fbready = 0;
    LTDC_LAYER(&hltdc_discovery, 0)->CFBAR = ((uint32_t)g_fblist[0]);
    __DSB();
    __HAL_LTDC_RELOAD_CONFIG(&hltdc_discovery);
    BSP_LCD_DisplayStringAtLine(0, (uint8_t*)msgbuf);

    while(1) // stay forever
    {
        // pump out the error message forever
        // in case UART is connected after the error occurs
        fprintf(stderr, "%s\n", msgbuf);
        for (int i = 0; i < 10; ++i)
        {
            BSP_LED_Toggle(LED1); // indicate error
            HAL_Delay_WFI(100);
        }
    }
}

void I_DoubleBufferEnable(int enable)
{
    g_double_buffer_enabled = enable ? true : false;
}

static int map_ascii_to_doom(uint8_t c)
{
    // normalize uppercase
    if (c >= 'A' && c <= 'Z')
        c += 'a' - 'A';

    switch (c)
    {
        // Movement
        case 'w': return KEY_UPARROW;
        case 's': return KEY_DOWNARROW;
        case 'a': return KEY_LEFTARROW;
        case 'd': return KEY_RIGHTARROW;

        // Strafing
        case 'q': return KEY_STRAFE_L;
        case 'e': return KEY_STRAFE_R;

        // Use & Fire
        case 'z': return KEY_USE;      // default was space
        case ' ': return KEY_FIRE;     // default was ctrl

        // Common controls
        case '\r': return KEY_ENTER;
        case 27:   return KEY_ESCAPE;  // ESC
        case '\t': return KEY_TAB;
        case '+':  return KEY_EQUALS;
        case '-':  return KEY_MINUS;
        case 'p':  return KEY_PAUSE;
        case 'm':  return 'm';         // automap
        case 't':  return 't';         // talk

        // Weapons
        case '1': return '1';
        case '2': return '2';
        case '3': return '3';
        case '4': return '4';
        case '5': return '5';
        case '6': return '6';
        case '7': return '7';
        case '8': return '8';
        case '9': return '9';

        default: return 0;
    }
}

static uint8_t keypressed[256]; // key pressed state
static uint32_t keypressedlast[256]; // key pressed time
int DG_GetKey(int* pressed, unsigned char* doomKey)
{
    uint32_t time = HAL_GetTick();

    while (g_uart_rx_buf_size > 0)
    {
        __disable_irq(); // disable interrupts
        g_uart_rx_buf_size--;
        uint8_t key = g_uart_rx_buf[g_uart_rx_buf_size];
        __enable_irq(); // enable interrupts

        key = map_ascii_to_doom(key);
        if (key)
        {
            keypressed[key] = 1;
            keypressedlast[key] = time; // record the time of the key press

            *pressed = 1; // key is pressed
            *doomKey = key;

            return 1; // key found
        }
    }

    int debounce = UART_KEY_HOLD_MS;
    for (int i=0; i<256; i++)
    {
        if (!keypressed[i])
            continue;

        // check if the key is still pressed
        if (time - keypressedlast[i] > debounce)
        {
            keypressed[i] = 0;
            *pressed = 0;
            *doomKey = i;
            return 1; // key is no longer pressed
        }
    }

    return 0;
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
        if (g_uart_rx_buf_size < UART_RX_BUF_SIZE)
        {
            g_uart_rx_buf[g_uart_rx_buf_size] = g_uart_rx_byte;
            g_uart_rx_buf_size++;
        }

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
        __HAL_RCC_DMA2_CLK_ENABLE();  // USART1 uses DMA2

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

        /* USART1 TX DMA Init */
        hdma_usart1_tx.Instance = DMA2_Stream7;
        hdma_usart1_tx.Init.Channel = DMA_CHANNEL_4;
        hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart1_tx.Init.Mode = DMA_NORMAL;
        hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
        hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        HAL_DMA_Init(&hdma_usart1_tx);

        __HAL_LINKDMA(uartHandle, hdmatx, hdma_usart1_tx);

        /* USART1 interrupt Init */
        HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{
    if(uartHandle->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
        HAL_DMA_DeInit(uartHandle->hdmatx);
        HAL_NVIC_DisableIRQ(USART1_IRQn);
    }
}

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* DMA2_Stream7_IRQn interrupt config (for USART1_TX) */
    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);
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

static void enable_dwt_cycle_counter(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    *((volatile uint32_t*)0xE0001FB0) = 0xC5ACCE55; // DWT_LAR unlock
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void HAL_Delay_WFI(uint32_t Delay)
{
    uint32_t tickstart = HAL_GetTick();
    uint32_t wait = Delay;

    /* Add a freq to guarantee minimum wait */
    if (wait < HAL_MAX_DELAY)
    {
        wait += (uint32_t)(uwTickFreq);
    }

    while ((HAL_GetTick() - tickstart) < wait)
    {
        __WFI(); // wait for interrupt
    }
}

