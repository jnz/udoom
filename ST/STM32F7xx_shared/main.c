/*
             | |
    _   _  __| | ___   ___  _ __ ___
   | | | |/ _` |/ _ \ / _ \| '_ ` _ \
   | |_| | (_| | (_) | (_) | | | | | |
    \__,_|\__,_|\___/ \___/|_| |_| |_|


   Doom for the STM32F7xx microcontroller
   */

/******************************************************************************
 * INCLUDE FILES
 ******************************************************************************/


#include <stdio.h>
#include <stdarg.h>
// Board specific includes
#include "stm32f7xx.h"

#include "main_stm32f7xx.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#include "memusage.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define UART_RX_BUF_SIZE     2   // must be power of 2
#define UART_KEY_HOLD_MS     100 // mark a key as released after xxx ms over uart

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/******************************************************************************
 * GLOBAL DATA DEFINITIONS
 ******************************************************************************/

// UART
static volatile uint8_t g_uart_rx_buf[UART_RX_BUF_SIZE];
static volatile int g_uart_rx_buf_size; // number of bytes in g_uart_rx_buf

/******************************************************************************
 * LOCAL DATA DEFINITIONS
 ******************************************************************************/

// Display/Framebuffer
static volatile int g_frame_ready = 0; // safe to swap buffers?
static uint32_t g_fblist[2]; // address of framebuffers
static volatile bool g_double_buffer_enabled = false; // initially set false
static volatile int g_fbcur = 1; // index into g_fblist, start in invisible buffer
static volatile uint32_t g_vsync_count;

// Self Monitoring
extern int gametic; // dooms internal timer from d_loop.c
static volatile uint32_t g_last_vsync;
static int g_last_seen_gametic; // monitor gametic
static uint32_t g_last_gametic_change_time; // timestamp of last gametic change

/******************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 ******************************************************************************/

// Profiler / Self-Monitoring
static void self_monitoring(void);

// Keyboard Input
static int map_ascii_to_doom(uint8_t c);

/******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

/******************************************************************************
 * FUNCTION BODIES
 ******************************************************************************/

static void board_common_init_post(void)
{
    g_fblist[0] = (uint32_t)I_FramebufferGet(0);
    g_fblist[1] = (uint32_t)I_FramebufferGet(1);
    g_last_vsync = HAL_GetTick();
    g_last_seen_gametic = gametic; // Self Monitoring
    g_last_gametic_change_time = HAL_GetTick();

    printf("STM32F7xx Doom\n"); // early sign of life
    printf("Core frequency: %lu MHz\n", HAL_RCC_GetHCLKFreq() / 1000000);
    printf("Total stack size: %u bytes\n", stack_total());
    printf("Total heap size: %u bytes\n", heap_total());
}

int main(void)
{
    I_BoardInit();
    board_common_init_post();

    char* argv[] = { "doom.exe" };
    doomgeneric_Create(1, argv);
    I_FramebufferClearAll();
    I_DoubleBufferEnable(1);

    uint32_t cyclecount = 0;
    uint32_t fpscounter = 0;
    uint32_t nextfpsupdate = HAL_GetTick() + 1000;

    while (1)
    {
        const uint32_t cyclestart = DWT->CYCCNT;
        __disable_irq();
        DG_ScreenBuffer = (pixel_t*)g_fblist[g_fbcur]; // prepare the framebuffer for drawing
        __enable_irq();
        doomgeneric_Tick();
        fpscounter++;

        self_monitoring();
        cyclecount += DWT->CYCCNT - cyclestart; // count cycles used for this frame

        if (HAL_GetTick() > nextfpsupdate) // emit some debug info to printf/UART
        {
            float cpuload = (float)cyclecount / HAL_RCC_GetHCLKFreq();
            if (cpuload > 1.0f) { cpuload = 1.0f; }
            printf("FPS%3i CPU%3u%% VID%3uHz Stack %u/%u Heap %u/%uKB Zone %u/%uM gametic: %i time %u\n",
                   fpscounter, (int)(cpuload * 100), g_vsync_count,
                   stack_usage(), stack_total(),
                   heap_usage()/1024, heap_total()/1024,
                   Z_ZoneUsage()/(1024*1024), Z_ZoneSize()/(1024*1024),
                   gametic, HAL_GetTick());
            fpscounter = 0;
            cyclecount = 0;
            g_vsync_count = 0;
            nextfpsupdate += 1000;
        }
        /* wait until VSYNC (HAL_LTDC_LineEventCallback) has consumed the
         * latest frame: */
        while (g_frame_ready) { __WFI(); }
    }
    return 0;
}

void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *hltdc)
{
    if (g_frame_ready) // ready to swap frames?
    {
        // activate the next framebuffer
        LTDC_LAYER(hltdc, 0)->CFBAR = ((uint32_t)g_fblist[g_fbcur]);
        __DSB();
        __HAL_LTDC_RELOAD_CONFIG(hltdc);

        if (g_double_buffer_enabled)
        {
            g_fbcur = 1 - g_fbcur;
        }
        g_frame_ready = 0;
    }
    g_last_vsync = HAL_GetTick();
    HAL_LTDC_ProgramLineEvent(hltdc, 0); // setup next VSYNC callback
    g_vsync_count++;
}

// called by Doom during init
void DG_Init()
{
    // give Doom something to draw to
    DG_ScreenBuffer = (pixel_t*)g_fblist[g_fbcur];
}

// called by Doom at the end of a frame
void DG_DrawFrame()
{
    g_frame_ready = 1; // indicate that we can swap the frame
}

extern uint8_t _zone_start; /* linker puts this into SDRAM */
extern uint8_t _zone_end;
/** @brief Give the address and size of the zone memory.  */
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

    if (already_quitting) { return; }
    already_quitting = true;

    va_start(argptr, error);
    vsnprintf(msgbuf, sizeof(msgbuf), error, argptr);
    va_end(argptr);

    g_frame_ready = 0;
    I_ErrorDisplay(msgbuf); // board specific error display

    const uint32_t time = HAL_GetTick();
    while(1)
    {
        // pump out the error message continuously
        // in case UART is connected after the error occurs
        fprintf(stderr, "I_Err %s\n", msgbuf);
        for (int i = 0; i < 200; ++i)
        {
            HAL_Delay_WFI(50);
        }

#if 0
        // after X seconds, reset the board
        if (time - HAL_GetTick() > 5000)
        {
            NVIC_SystemReset();
        }
#endif
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

void enable_dwt_cycle_counter(void)
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

static void self_monitoring(void)
{
    uint32_t time = HAL_GetTick();
    if (g_last_seen_gametic != gametic)
    {
        g_last_seen_gametic = gametic;
        g_last_gametic_change_time = time;
    }
    const uint32_t gametic_age_ms = time - g_last_gametic_change_time;
    if (gametic_age_ms > 5000)
    {
        NVIC_SystemReset(); // Doom internal error, just reset
        // I_Error("gametic %i@%u ms (HAL_GetTick)", gametic, time);
    }

    if (time - g_last_vsync > 10000)
    {
        I_Error("VSYNC err %u/%u ms",
                time, g_last_vsync);

    }
}

void I_StdinByteRecv(uint8_t byte) /* called from an ISR */
{
    if (g_uart_rx_buf_size < UART_RX_BUF_SIZE)
    {
        g_uart_rx_buf[g_uart_rx_buf_size] = byte;
        g_uart_rx_buf_size++;
    }
}

