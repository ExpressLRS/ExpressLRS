/*
 *******************************************************************************
 * Copyright (c) 2019, STMicroelectronics
 * All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */

#include "pins_arduino.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

// Digital PinName array
// This array allows to wrap Arduino pin number(Dx or x)
// to STM32 PinName (PX_n)
const PinName digitalPin[] = {
  PA_10, //D0
  PA_9,  //D1
  PA_12, //D2
  PB_3,  //D3
  PB_5,  //D4
  PA_15, //D5
  PB_10, //D6
  PC_7,  //D7
  PB_6,  //D8
  PA_8,  //D9
  PA_11, //D10
  PB_15, //D11
  PB_14, //D12
  PB_13, //D13 - LED
  PB_7,  //D14
  PB_8,  //D15
  // ST Morpho
  // CN5 Left Side
  PC_10, //D16
  PC_12, //D17
  PB_12, //D18
  PA_13, //D19
  PA_14, //D20
  PC_13, //D21 - User Button
  PC_14, //D22
  PC_15, //D23
  PH_0,  //D24
  PH_1,  //D25
  PB_4,  //D26
  PB_9,  //D27
  // CN5 Right Side
  PC_11, //D28
  PD_2,  //D29
  // CN6 Left Side
  PC_9,  //D30
  // CN6 Right Side
  PC_8,  //D31
  PC_6,  //D32
  PC_5,  //D33
  PB_0,  //D34
  PA_10, //D35
  PA_9,  //D36
  PB_11, //D37
  PB_2,  //D38
  PB_1,  //D39
  PA_7,  //D40
  PA_6,  //D41
  PA_5,  //D42
  PA_4,  //D43
  PC_4,  //D44
  PA_3,  //D45 - STLink Rx
  PA_2,  //D46 - STLink Tx
  PA_0,  //D47/A0
  PA_1,  //D48/A1
  PC_3,  //D49/A2
  PC_2,  //D50/A3
  PC_1,  //D51/A4
  PC_0   //D52/A5
};

// If analog pins are not contiguous in the digitalPin array:
// Add the analogInputPin array without defining NUM_ANALOG_FIRST
// Analog (Ax) pin number array
// where x is the index to retrieve the digital pin number
const uint32_t analogInputPin[] = {
  47, //A0
  48, //A1
  49, //A2
  50, //A3
  51, //A4
  52, //A5
  34, //A6
  39, //A7
  40, //A8
  41, //A9
  42, //A10
  43  //A11
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
  *            PLL_Q                          = 2
  *            Flash Latency(WS)              = 4
  * @param  None
  * @retval None
  */
void SystemClock_Config(void)
{

  RCC_ClkInitTypeDef RCC_ClkInitStruct = {};
  RCC_OscInitTypeDef RCC_OscInitStruct = {};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {};

  memset(&RCC_OscInitStruct, 0, sizeof(RCC_OscInitTypeDef));
  memset(&RCC_ClkInitStruct, 0, sizeof(RCC_ClkInitTypeDef));
  memset(&PeriphClkInit, 0, sizeof(RCC_PeriphCLKInitTypeDef));

  /* MSI is enabled after System reset, activate PLL with MSI as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_OFF;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV4;
  RCC_OscInitStruct.HSICalibrationValue = 0x10;

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

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1 | RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    while (1);
  }

  HAL_ResumeTick();
}


#ifdef __cplusplus
}
#endif
