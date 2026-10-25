/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32v30x_it.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2022/01/18
 * Description        : Main Interrupt Service Routines for CH32V307 KMBox Net.
 *******************************************************************************/
#include "eth_driver.h"
#include "ch32v30x_it.h"

#if defined(__riscv_xw)
#define WCH_FAST_IRQ __attribute__((interrupt("WCH-Interrupt-fast")))
#else
#define WCH_FAST_IRQ __attribute__((interrupt))
#endif

void NMI_Handler(void) WCH_FAST_IRQ;
void HardFault_Handler(void) WCH_FAST_IRQ;
void ETH_IRQHandler(void) WCH_FAST_IRQ;
void TIM2_IRQHandler(void) WCH_FAST_IRQ;
void EXTI9_5_IRQHandler(void) WCH_FAST_IRQ;

/*********************************************************************
 * @fn      NMI_Handler
 *
 * @brief   This function handles NMI exception.
 */
void NMI_Handler(void)
{
    while (1)
    {
    }
}

/*********************************************************************
 * @fn      HardFault_Handler
 *
 * @brief   This function handles Hard Fault exception.
 */
void HardFault_Handler(void)
{
    NVIC_SystemReset();
    while (1)
    {
    }
}

/*********************************************************************
 * @fn      EXTI9_5_IRQHandler
 *
 * @brief   Internal 10M Ethernet PHY link status interrupt handler.
 */
void EXTI9_5_IRQHandler(void)
{
    ETH_PHYLink();
    EXTI_ClearITPendingBit(EXTI_Line7);
}

/*********************************************************************
 * @fn      ETH_IRQHandler
 *
 * @brief   WCHNET Ethernet frame RX/TX interrupt handler.
 */
void ETH_IRQHandler(void)
{
    WCHNET_ETHIsr();
}

/*********************************************************************
 * @fn      TIM2_IRQHandler
 *
 * @brief   WCHNET periodic timer interrupt handler.
 */
void TIM2_IRQHandler(void)
{
    WCHNET_TimeIsr(WCHNETTIMERPERIOD);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
}
