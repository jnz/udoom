#include "stm32f7xx.h"
#include "doomkeys.h"
#include "doomgeneric.h"

void DG_SleepMs(uint32_t ms)
{
    HAL_Delay(ms);
}

uint32_t DG_GetTicksMs()
{
    return HAL_GetTick();
}

void DG_SetWindowTitle(const char * title)
{

}

/* moved to main.c */
/*
void DG_DrawFrame() { }
void DG_Init() { }
int DG_GetKey(int* pressed, unsigned char* doomKey) {}
*/

