#include <stdint.h>
#include "stm32f7xx.h"
#include "stm32f769i_discovery.h"
#include "stm32f769i_discovery_lcd.h"
#include "stm32f769i_discovery_sdram.h"

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

    int x = 0;
    int y = 0;
    int dirx = 3;
    int diry = 3;
    while (1)
    {
        volatile uint32_t* drawFB = (uint32_t*)g_fblist[g_fbcur];

        memset((void *)drawFB, 0, FRAMEBUFFER_HEIGHT * FRAMEBUFFER_WIDTH * BYTES_PER_PIXEL);
        drawFB[y*FRAMEBUFFER_WIDTH + x] = LCD_COLOR_WHITE;
        x+=dirx;
        y+=diry;

        if (x >= FRAMEBUFFER_WIDTH) {dirx *= -1; x = FRAMEBUFFER_WIDTH-1; }
        if (y >= FRAMEBUFFER_HEIGHT) {diry*= -1; y = FRAMEBUFFER_HEIGHT-1; }
        if (x < 0) { dirx *= -1; x = 0; }
        if (y < 0) { diry *= -1; y = 0; }

        // SCB_CleanDCache_by_Addr((uint32_t*)drawFB, FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * BYTES_PER_PIXEL);
        g_fbready = 1; // indicate that we can swap the frame
        while (g_fbready)
        {
            HAL_Delay(0); // wait until frame swap
        }
    }

    return 0;
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
