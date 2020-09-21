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

// Pin number
const PinName digitalPin[] = {
  PA_10, //D0
  PA_9,  //D1
  PA_12, //D2
  PB_0,  //D3/A8
  PB_7,  //D4
  PB_6,  //D5
  PB_1,  //D6/A9
  PF_0,  //D7
  PF_1,  //D8
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
  PA_15  //D22    - STLink Rx
};

// If analog pins are not contiguous in the digitalPin array:
// Add the analogInputPin array without defining NUM_ANALOG_FIRST
// Analog (Ax) pin number array
// where x is the index to retrieve the digital pin number
const uint32_t analogInputPin[] = {
  14, //A0
  15, //A1
  16, //A2
  17, //A3
  18, //A4
  19, //A5
  20, //A6
  21, //A7
  3,  //A8
  6   //A9
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
  * @param  None
  * @retval None
  */
// WEAK void SystemClock_Config(void)
// {
  // RCC_OscInitTypeDef RCC_OscInitStruct;
  // RCC_ClkInitTypeDef RCC_ClkInitStruct;
  // RCC_PeriphCLKInitTypeDef PeriphClkInit;

  // /* Initializes the CPU, AHB and APB busses clocks */
  // RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  // RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  // RCC_OscInitStruct.HSICalibrationValue = 16;
  // RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  // RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  // RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  // if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    // while (1);
  // }

  // /* Initializes the CPU, AHB and APB busses clocks */
  // RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                // | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  // RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  // RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  // RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  // RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  // if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    // while (1);
  // }

  // PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1 | RCC_PERIPHCLK_ADC12;
  // PeriphClkInit.Adc12ClockSelection = RCC_ADC12PLLCLK_DIV1;
  // PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_SYSCLK;
  // if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    // while (1);
  // }
// }

// static void MX_SPI3_Init(void)
// {

  // /* USER CODE BEGIN SPI3_Init 0 */

  // /* USER CODE END SPI3_Init 0 */

  // /* USER CODE BEGIN SPI3_Init 1 */

  // /* USER CODE END SPI3_Init 1 */
  // /* SPI3 parameter configuration*/
  // hspi3.Instance = SPI3;
  // hspi3.Init.Mode = SPI_MODE_MASTER;
  // hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  // hspi3.Init.DataSize = SPI_DATASIZE_4BIT;
  // hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  // hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  // hspi3.Init.NSS = SPI_NSS_SOFT;
  // hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  // hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  // hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  // hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  // hspi3.Init.CRCPolynomial = 7;
  // hspi3.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  // hspi3.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  // if (HAL_SPI_Init(&hspi3) != HAL_OK)
  // {
    // Error_Handler();
  // }
  // /* USER CODE BEGIN SPI3_Init 2 */

  // /* USER CODE END SPI3_Init 2 */

// }

WEAK void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV4;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  
  //MX_SPI3_Init();
}

#ifdef __cplusplus
}
#endif
