/*
  Copyright (c) 2011 Arduino.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "pins_arduino.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

// Pin number
const PinName digitalPin[] = {
  PA_10, //D0
  PA_9,  //D1
  PA_12, //D2
  PB_0,  //D3
  PB_7,  //D4
  PB_6,  //D5
  PB_1,  //D6
  PC_14, //D7 - By default, SB6 open PF0/PC14 not connected to D7
  PC_15, //D8 - By default, SB8 open PF1/PC15 not connected to D8
  PA_8,  //D9
  PA_11, //D10
  PB_5,  //D11
  PB_4,  //D12
  PB_3,  //D13 - LED
  PA_0,  //D14/A0
  PA_1,  //D15/A1
  PA_3,  //D16/A2
  PA_4,  //D17/A3
  PA_5,  //D18/A4
  PA_6,  //D19/A5
  PA_7,  //D20/A6
  PA_2,  //D21/A7 - STLink Tx
  PA_15  //D22 - STLink Rx
};

// If analog pins are not contiguous in the digitalPin array:
// Add the analogInputPin array without defining NUM_ANALOG_FIRST
// Analog (Ax) pin number array
// where x is the index to retrieve the digital pin number
const uint32_t analogInputPin[] = {
  PA0,
  PA1,
  PA3,
  PA4,
  PA5,
  PA6,
  PA7
};

#ifdef __cplusplus
}
#endif

// ----------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follows :
  *            System Clock source            = PLL (MSI)
  *            SYSCLK(Hz)                     = 80000000
  *            HCLK(Hz)                       = 80000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 1
  *            APB2 Prescaler                 = 1
  *            MSI Frequency(Hz)              = 4000000
  *            PLL_M                          = 1
  *            PLL_N                          = 40
  *            PLL_R                          = 2
  *            PLL_P                          = 7
  *            PLL_Q                          = 4
  *            Flash Latency(WS)              = 4
  * @param  None
  * @retval None
  */
WEAK void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {};
  RCC_OscInitTypeDef RCC_OscInitStruct = {};

#if USE_HSI
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {};

  memset(&RCC_OscInitStruct, 0, sizeof(RCC_OscInitTypeDef));
  memset(&RCC_ClkInitStruct, 0, sizeof(RCC_ClkInitTypeDef));
  memset(&PeriphClkInit, 0, sizeof(RCC_PeriphCLKInitTypeDef));

  /* Enable Power Control clock */
  __HAL_RCC_PWR_CLK_ENABLE();

  /* The voltage scaling allows optimizing the power consumption when the device is
     clocked below the maximum system frequency, to update the voltage scaling value
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /* Enable HSI Oscillator and activate PLL with HSI as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_OFF;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;  // 16MHz
  RCC_OscInitStruct.PLL.PLLN = 10; // 10 * 16MHz
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2; // 160MHz / 2
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7; // 160MHz / 7
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV4; // 160MHz / 4
  RCC_OscInitStruct.HSICalibrationValue = 0x10;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    //Error_Handler();
    while (1);

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    //Error_Handler();
    while (1);

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1 | RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    //Error_Handler();
    while (1);

  HAL_ResumeTick();


#else // !USE_HSI

  /* MSI is enabled after System reset, activate PLL with MSI as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;

  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLP = 7;
  RCC_OscInitStruct.PLL.PLLQ = 4;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    /* Initialization Error */
    while (1);
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
    /* Initialization Error */
    while (1);
  }

#endif // USE_HSI
}

void initVariant(void)
{
  /* Reset vector location which is set wrongly by SystemInit */
  extern uint32_t g_pfnVectors;
  SCB->VTOR = (uint32_t) &g_pfnVectors;
}

#ifdef __cplusplus
}
#endif
