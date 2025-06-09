/**
  ******************************************************************************
  * @file    Templates/Src/stm32f7xx.c
  * @author  MCD Application Team
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"
#include "stm32f7xx_it.h"
#ifdef STM32F769xx
#include "stm32f769i_discovery.h"
extern LTDC_HandleTypeDef hltdc_discovery;
static LTDC_HandleTypeDef* phltdc = &hltdc_discovery;
extern DMA_HandleTypeDef hdma_usart1_tx;
#endif
#ifdef STM32F750xx
#include "stm32f7508_discovery.h"
extern LTDC_HandleTypeDef hLtdcHandler;
static LTDC_HandleTypeDef* phltdc = &hLtdcHandler;
#endif

/** @addtogroup STM32F7xx_HAL_Examples
  * @{
  */

/** @addtogroup Templates
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern SD_HandleTypeDef uSdHandle;
extern UART_HandleTypeDef huart1;
extern DMA2D_HandleTypeDef hdma2d;
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M7 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief   This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
    __asm("BKPT #0");
    /* Go to infinite loop when Hard Fault exception occurs */
    while (1)
    {
        // Stay a while – stay forever!
        __WFI();
    }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
    /* Go to infinite loop when Memory Manage exception occurs */
    while (1)
    {
        __WFI(); // at least save some power
    }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
  HAL_IncTick();
}

/******************************************************************************/
/*                 STM32F7xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f7xx.s).                                               */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

void DMA2D_IRQHandler(void)
{
    HAL_DMA2D_IRQHandler(&hdma2d);
}

void LTDC_IRQHandler(void)
{
    HAL_LTDC_IRQHandler(phltdc);
}

/**
 * @brief Handles SDMMC2 DMA Rx transfer interrupt request.
 * @retval None
 */
void BSP_SDMMC2_DMA_Rx_IRQHandler(void)
{
    HAL_DMA_IRQHandler(uSdHandle.hdmarx);
}

void BSP_SDMMC_DMA_Rx_IRQHandler(void)
{
    HAL_DMA_IRQHandler(uSdHandle.hdmarx);
}

/**
 * @brief Handles SDMMC2 DMA Tx transfer interrupt request.
 * @retval None
 */
void BSP_SDMMC2_DMA_Tx_IRQHandler(void)
{
    HAL_DMA_IRQHandler(uSdHandle.hdmatx);
}

void BSP_SDMMC_DMA_Tx_IRQHandler(void)
{
    HAL_DMA_IRQHandler(uSdHandle.hdmatx);
}

/**
 * @brief Handles SD1 card interrupt request.
 * @retval None
 */
void BSP_SDMMC2_IRQHandler(void)
{
    HAL_SD_IRQHandler(&uSdHandle);
}

void BSP_SDMMC_IRQHandler(void)
{
    HAL_SD_IRQHandler(&uSdHandle);
}

/**
 * @brief This function handles DMA2 stream7 global interrupt.
 */
#ifdef STM32F769xx
void DMA2_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}
#endif

/**
 * @brief This function handles USART1 global interrupt.
 */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

/**
  * @}
  */

