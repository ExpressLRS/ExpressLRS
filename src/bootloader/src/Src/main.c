/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include <string.h>

/* Private includes ----------------------------------------------------------*/
#include "main.h"
#include "uart.h"
#include "flash.h"
#if (TARGET_XMODEM == 1)
#include "xmodem.h"
#else
#if STK500
#include "stk500.h"
#else // STK500
#include "frsky.h"
#endif // STK500
#endif // TARGET_XMODEM

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);

/*static void delay(int x) {
    for (int i = 0; i < x; i++)
        for (int j = 0; j < 500; j++)
            __asm__("nop");
}
*/

/* Private user code ---------------------------------------------------------*/

void led_red_state_set(const GPIO_PinState state)
{
#ifdef LED_RED_Pin
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, state);
#else
  (void)state;
#endif
}

void led_green_state_set(const GPIO_PinState state)
{
#ifdef LED_GRN_Pin
  HAL_GPIO_WritePin(LED_GRN_GPIO_Port, LED_GRN_Pin, state);
#else
  (void)state;
#endif
}

void duplex_state_set(const enum duplex_state state)
{
#ifdef DUPLEX_Pin
  HAL_GPIO_WritePin(DUPLEX_Port, DUPLEX_Pin, (state == DUPLEX_TX));
#else
  (void)state;
#endif
}

#if (TARGET_XMODEM == 1)
static void boot_code(void)
{
#ifdef BTN_Pin
  int i;
#endif
  bool BLrequested = false;
  uint8_t header[6] = {0, 0, 0, 0, 0, 0};

  led_red_state_set(1);
  led_green_state_set(1);

  /* Send welcome message on startup. */
  uart_transmit_str((uint8_t *)"\n\r==========================================\n\r");
  uart_transmit_str((uint8_t *)"  UART Bootloader for ExpressLRS\n\r");
  uart_transmit_str((uint8_t *)"  https://github.com/AlessandroAU/ExpressLRS\n\r");
  uart_transmit_str((uint8_t *)"==========================================\n\r\n\r");
  /* If the button is pressed, then jump to the user application,
   * otherwise stay in the bootloader. */
  uart_transmit_str((uint8_t *)"Send '2bl', 'bbb' or hold down button to begin bootloader\n\r");

  uart_receive(header, 5u);

  /* Search for magic strings */
  if (strstr((char *)header, "2bl") || strstr((char *)header, "bbb"))
  {
    BLrequested = true;
  }
#ifdef BTN_Pin
  // Wait button press to access bootloader
  if (!BLrequested) // Check UART if not button not pressed
    for (i = 0; i < 2; i++)
    {
      if (HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin) == GPIO_PIN_RESET)
      {
        HAL_Delay(100); // wait debounce
        if (HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin) == GPIO_PIN_RESET)
        {
          // Button still pressed
          uart_transmit_str((uint8_t *)"Button press\n\r");
          BLrequested = true;
          break;
        }
      }
      else
      {
        HAL_Delay(50);
      }
    }
#endif /* BTN_Pin */

  if (BLrequested == true)
  {
    /* BL was requested, GRN led on */
    led_red_state_set(0);
    led_green_state_set(1);
  }
  else
  {
    /* BL was not requested, RED led on. Use app will soon use the LED's for
     * it's own purpose, thus if RED stays on there is an error */
    led_red_state_set(1);
    led_green_state_set(0);
    uart_transmit_str((uint8_t *)"Start app\n\r");
    flash_jump_to_app();
  }

  /* Infinite loop */
  while (1)
  {

    /* Turn on the green LED to indicate, that we are in bootloader mode.*/
    // HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
    /* Ask for new data and start the Xmodem protocol. */
    // uart_transmit_str((uint8_t*)"Please send a new binary file with Xmodem
    // protocol to update the firmware.\n\r");
    xmodem_receive();
    /* We only exit the xmodem protocol, if there are any errors.
     * In that case, notify the user and start over. */
    //uart_transmit_str((uint8_t *)"\n\rFailed... Please try again.\n\r");
  }
}

#else // TARGET_XMODEM == 0

#define BOOT_WAIT 300 // ms

static uint32_t boot_end_time;

int8_t timer_end(void)
{
  //return 0;
  return (HAL_GetTick() > boot_end_time);
}

static void boot_code(void)
{
  boot_end_time = HAL_GetTick() + BOOT_WAIT;

  /* Infinite loop */
  while (1)
  {
    /* Check if update is requested */
    if (
#if STK500
        stk500_check() < 0
#else
        frsky_check() < 0
#endif
    )
    {
      flash_jump_to_app();
      while (1)
        ;
    }
  }
}

#endif /* TARGET_XMODEM */

extern int FLASH_START;

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
  /* MCU Configuration---------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the
   * Systick.
   */
  SCB->VTOR = (uint32_t)&FLASH_START;
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();
  __enable_irq();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();

  uart_init();

  boot_code();
}

#if defined(STM32L0xx) || defined(STM32L4xx)
void SysTick_Handler(void)
{
  HAL_IncTick();
  HAL_SYSTICK_IRQHandler();
}
#endif

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  memset(&RCC_OscInitStruct, 0, sizeof(RCC_OscInitTypeDef));
  memset(&RCC_ClkInitStruct, 0, sizeof(RCC_ClkInitTypeDef));

#if defined(STM32L0xx) || defined(STM32L4xx)
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
#if defined(STM32L4xx)
  RCC_OscInitStruct.PLL.PLLM = 1;  // 16MHz
  RCC_OscInitStruct.PLL.PLLN = 10; // 10 * 16MHz
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2; // 160MHz / 2
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7; // 160MHz / 7
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2; // 160MHz / 2
#else
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL4;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLL_DIV2;
#endif
  RCC_OscInitStruct.HSICalibrationValue = 0x10;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    Error_Handler();

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
#if defined(STM32L4xx)
  uint32_t flash_latency = FLASH_LATENCY_4;
#else
  uint32_t flash_latency = FLASH_LATENCY_1;
#endif
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, flash_latency) != HAL_OK)
    Error_Handler();

  RCC_PeriphCLKInitTypeDef PeriphClkInit = {};
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1 | RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    Error_Handler();

  HAL_ResumeTick();

#elif defined(STM32L1xx)
#warning "Clock setup is missing! Should use HSI!"

#elif defined(STM32F1)

  /** Initializes the CPU, AHB and APB busses clocks
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }
#endif
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
#if defined(BTN_Pin) || defined(LED_GRN_Pin) || defined(LED_RED_Pin) || defined(DUPLEX_Pin)
  GPIO_InitTypeDef GPIO_InitStruct;
#endif

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
#ifndef TARGET_RAK4200
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
#endif

#ifdef BTN_Pin
  /*Configure GPIO pin : BTN_Pin */
  GPIO_InitStruct.Pin = BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BTN_GPIO_Port, &GPIO_InitStruct);
#endif

#ifdef LED_GRN_Pin
  /*Configure GPIO pin : LED_GRN_Pin */
  GPIO_InitStruct.Pin = LED_GRN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GRN_GPIO_Port, &GPIO_InitStruct);
  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GRN_GPIO_Port, LED_GRN_Pin, GPIO_PIN_RESET);
#endif

#ifdef LED_RED_Pin
  /*Configure GPIO pin : LED_RED_Pin */
  GPIO_InitStruct.Pin = LED_RED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_RED_GPIO_Port, &GPIO_InitStruct);
  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
#endif

#ifdef DUPLEX_Pin
  /*Configure GPIO pin : DUPLEX_Pin */
  GPIO_InitStruct.Pin = DUPLEX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DUPLEX_Port, &GPIO_InitStruct);
  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DUPLEX_Port, DUPLEX_Pin, GPIO_PIN_RESET);
#endif
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* User can add his own implementation to report the HAL error return state
   */
  while (1)
    ;
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line
     number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line)
   */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
