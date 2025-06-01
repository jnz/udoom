#include "stm32f7xx.h"
#include "stm32f7xx_hal.h"
#include "stm32f7508_discovery.h"
#include "stm32f7508_discovery_qspi.h"

typedef void (*pFunction)(void);

static void SystemClock_Config(void);
static void JumpToQSPI(uint32_t address);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    BSP_LED_Init(LED1);
    BSP_LED_On(LED1);

    if (BSP_QSPI_Init() != QSPI_OK)
        while (1);
    if (BSP_QSPI_EnableMemoryMappedMode() != QSPI_OK)
        while (1);

    uint32_t qspi_sp = *(volatile uint32_t*)0x90000000;
    uint32_t qspi_pc = *(volatile uint32_t*)0x90000004;

    if ((qspi_sp & 0x2FFE0000) != 0x20000000)
        while (1); // Invalid SP → stay here

    JumpToQSPI(0x90000000);
    while (1);
}

static void JumpToQSPI(uint32_t address)
{
    __disable_irq();
    SCB->VTOR = address;
    __set_MSP(*(volatile uint32_t*)address);
    ((pFunction)(*(volatile uint32_t*)(address + 4)))();
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 400;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 8;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6);
}
