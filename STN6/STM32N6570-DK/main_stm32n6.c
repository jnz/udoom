/*
             | |
    _   _  __| | ___   ___  _ __ ___
   | | | |/ _` |/ _ \ / _ \| '_ ` _ \
   | |_| | (_| | (_) | (_) | | | | | |
    \__,_|\__,_|\___/ \___/|_| |_| |_|


   Doom for the STM32N6570 microcontroller
   */

/******************************************************************************
 * INCLUDE FILES
 ******************************************************************************/

#include <stdio.h>
#include <stdarg.h>
// Board specific includes
#include "stm32n6xx_hal.h"
#include "stm32n6570_discovery.h"

// Doom includes
#include "doomkeys.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/******************************************************************************
 * GLOBAL DATA DEFINITIONS
 ******************************************************************************/

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
    // stack_fill_with_magic();

    // LEDs
    BSP_LED_Init(LED1);
    BSP_LED_Init(LED2);
    BSP_LED_On(LED1);

    while (1)
    {
        BSP_LED_Toggle(LED1);
        HAL_Delay(500);
        BSP_LED_Toggle(LED2);
        HAL_Delay(500);
    }
}

/** @brief  CPU L1-Cache enable.  */
static void CPU_CACHE_Enable(void)
{

}

static void Error_Handler(void)
{
    while(1) { ; }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the System Power Supply
    */
    if (HAL_PWREx_ConfigSupply(PWR_EXTERNAL_SOURCE_SUPPLY) != HAL_OK)
    {
        Error_Handler();
    }

    /* Enable HSI */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_NONE;
    RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
    RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
    RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Get current CPU/System buses clocks configuration and if necessary switch
      to intermediate HSI clock to ensure target clock can be set
      */
    HAL_RCC_GetClockConfig(&RCC_ClkInitStruct);
    if ((RCC_ClkInitStruct.CPUCLKSource == RCC_CPUCLKSOURCE_IC1) ||
        (RCC_ClkInitStruct.SYSCLKSource == RCC_SYSCLKSOURCE_IC2_IC6_IC11))
    {
        RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK);
        RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_HSI;
        RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
        if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
        {
            /* Initialization Error */
            Error_Handler();
        }
    }

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_NONE;
    RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL1.PLLM = 4;
    RCC_OscInitStruct.PLL1.PLLN = 75;
    RCC_OscInitStruct.PLL1.PLLFractional = 0;
    RCC_OscInitStruct.PLL1.PLLP1 = 1;
    RCC_OscInitStruct.PLL1.PLLP2 = 1;
    RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
    RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
    RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_CPUCLK|RCC_CLOCKTYPE_HCLK
        |RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
        |RCC_CLOCKTYPE_PCLK2|RCC_CLOCKTYPE_PCLK5
        |RCC_CLOCKTYPE_PCLK4;
    RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
    RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV1;
    RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    RCC_ClkInitStruct.IC1Selection.ClockDivider = 2;
    RCC_ClkInitStruct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    RCC_ClkInitStruct.IC2Selection.ClockDivider = 3;
    RCC_ClkInitStruct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    RCC_ClkInitStruct.IC6Selection.ClockDivider = 4;
    RCC_ClkInitStruct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    RCC_ClkInitStruct.IC11Selection.ClockDivider = 3;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }
}

__attribute__((weak)) uint32_t SECURE_SystemCoreClockUpdate(void) { return 0; }

static void MPU_Config(void)
{

}

void I_ErrorDisplay(const char* msg)
{

}

int main(void)
{
    I_BoardInit();
    return 0;
}

