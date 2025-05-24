#include <stdint.h>
#include "stm32f7xx.h"
#include "stm32f769i_discovery.h"
#include "stm32f769i_discovery_lcd.h"
#include "stm32f769i_discovery_sdram.h"

#define LCD_FRAMEBUFFER ((uint32_t)0xC0000000)
#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 272

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

    BSP_LCD_LayerDefaultInit(0, LCD_FRAMEBUFFER);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_Clear(LCD_COLOR_BLACK);

    while (1)
    {
        BSP_LED_Toggle(LED2);
        HAL_Delay(100);
    }
}
