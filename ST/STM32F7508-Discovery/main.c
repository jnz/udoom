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
#include <string.h>
#include <stdarg.h>
// Board specific includes
#include "stm32f7xx.h"
#include "stm32f7508_discovery.h"
#include "stm32f7508_discovery_lcd.h"
#include "stm32f7508_discovery_sdram.h"
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
#define LCD_WIDTH_PIXEL      RK043FN48H_WIDTH
#define LCD_HEIGHT_PIXEL     RK043FN48H_HEIGHT

// UART
#define UART_RX_BUF_SIZE     2   // must be power of 2
#define UART_KEY_HOLD_MS     100 // mark a key as released after xxx ms over uart

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/******************************************************************************
 * GLOBAL DATA DEFINITIONS
 ******************************************************************************/

// Double Buffering
__attribute__((section(".framebuffer1"))) uint32_t framebuffer1[LCD_WIDTH_PIXEL * LCD_HEIGHT_PIXEL];
__attribute__((section(".framebuffer2"))) uint32_t framebuffer2[LCD_WIDTH_PIXEL * LCD_HEIGHT_PIXEL];
extern LTDC_HandleTypeDef hLtdcHandler;
static LTDC_HandleTypeDef* phltdc = &hLtdcHandler;
extern pixel_t* DG_ScreenBuffer; // buffer for doom to draw to

// UART
UART_HandleTypeDef  huart1;

/******************************************************************************
 * LOCAL DATA DEFINITIONS
 ******************************************************************************/

// Double Buffering
static uint32_t g_fblist[2];
static uint32_t g_vsync_count;
volatile static bool g_double_buffer_enabled = false; // initially set to false

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

int app_main(void)
{
    MPU_Config();
    CPU_CACHE_Enable();
    HAL_Init();
    SystemClock_Config();
    enable_dwt_cycle_counter();

    // LEDs
    BSP_LED_Init(LED1);
    BSP_LED_On(LED1);

    // SDRAM
    BSP_SDRAM_Init();

    // UART
    MX_USART1_UART_Init();
    printf("STM32F7508 Doom\n"); // early sign of life
    printf("Core frequency: %lu MHz\n", HAL_RCC_GetHCLKFreq() / 1000000);

    // Display
    g_fblist[0] = (uint32_t)framebuffer1;
    g_fblist[1] = (uint32_t)framebuffer2;
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, g_fblist[0]);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_SetLayerVisible(0, ENABLE);
    BSP_LCD_SetTransparency(0, 255);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
    // BSP_LCD_SetBrightness(100); // set brightness to 100%
    Framebuffer_Clear();
    // Enable VSYNC interrupts
    HAL_NVIC_SetPriority(LTDC_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(LTDC_IRQn);

    // Initial hello
    int line = 0;
    BSP_LCD_DisplayStringAtLine(line++, "STM32F7508 Doom by Jan Zwiener");
    BSP_LED_Off(LED1); // MCU init complete

    // Prepare main loop
    char *argv[] = { "doom.exe" };
    doomgeneric_Create(sizeof(argv)/sizeof(argv[0]), argv);
    Framebuffer_Clear();

    I_DoubleBufferEnable(1);
    HAL_LTDC_ProgramLineEvent(phltdc, 0);
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
            float cpuload = (float)cyclecount / HAL_RCC_GetHCLKFreq();
            if (cpuload > 1.0f) { cpuload = 1.0f; }
            printf("FPS %2i CPU%3u%% VSYNC%3u Hz\n",
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
    memset((void *)g_fblist[0], 0x00, sizeof(framebuffer1));
    memset((void *)g_fblist[1], 0x00, sizeof(framebuffer2));
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
        BSP_LED_Toggle(LED1); /* some developer feedback */
    }
    HAL_LTDC_ProgramLineEvent(hltdc, 0); // setup next VSYNC callback
    g_vsync_count++;
}

/** @brief Give the address and size of the zone memory.  */
extern uint8_t _zone_start;
extern uint8_t _zone_end;
uint8_t *I_ZoneBase (int *size)
{
    *size = (int)(&_zone_end - &_zone_start);
    return &_zone_start;
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
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_SetBackColor(LCD_COLOR_RED);
    __HAL_LTDC_DISABLE_IT(phltdc, LTDC_IT_LI);
    g_fbready = 0;
    LTDC_LAYER(phltdc, 0)->CFBAR = ((uint32_t)g_fblist[0]);
    __DSB();
    __HAL_LTDC_RELOAD_CONFIG(phltdc);
    BSP_LCD_DisplayStringAtLine(0, (uint8_t*)msgbuf);

    while(1) // stay forever
    {
        // pump out the error message forever
        // in case UART is connected after the error occurs
        fprintf(stderr, "I_Error: %s\n", msgbuf);
        for (int i = 0; i < 100; ++i)
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
/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 200000000
  *            HCLK(Hz)                       = 200000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 4
  *            APB2 Prescaler                 = 2
  *            HSE Frequency(Hz)              = 25000000
  *            PLL_M                          = 25
  *            PLL_N                          = 400 / 432
  *            PLL_P                          = 2
  *            PLL_Q                          = 8
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale1 mode
  *            Flash Latency(WS)              = 6
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;
  HAL_StatusTypeDef ret = HAL_OK;

  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

#if (SYSTEM_FREQ == 216)
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 432;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 8;
#else
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 400;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 8;
#endif

  ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
  if(ret != HAL_OK)
  {
    while(1) { ; }
  }

  /* Activate the OverDrive to reach the 200 MHz Frequency */
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

  /* Configure QSPI region */
  MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress      = 0x90000000;
  MPU_InitStruct.Size             = MPU_REGION_SIZE_16MB; // or less, if you know it
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable      = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.Number           = MPU_REGION_NUMBER1;
  MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  // Region 2: FMC Control Registers
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0xA0000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_8KB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;  // Wichtig: NOT cacheable!
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER2;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  // Region 3: SDRAM Data (8MB)
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0xC0000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_8MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER3;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enable the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

static void MX_USART1_UART_Init(void)
{
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    BSP_COM_Init(COM1, &huart1);
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

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

