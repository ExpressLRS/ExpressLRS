/**
 * @file    uart.c
 * @author  Ferenc Nemeth
 * @date    21 Dec 2018
 * @brief   This module is a layer between the HAL UART functions and my Xmodem protocol.
 *
 *          Copyright (c) 2018 Ferenc Nemeth - https://github.com/ferenc-nemeth
 */

#include "uart.h"
#include "main.h"
#include <string.h>

static UART_HandleTypeDef huart1;

#ifdef DEBUG_UART
#if (DEBUG_UART == UART_NUM)
#error "Same uart cannot be used for debug and comminucation!"
#endif

UART_HandleTypeDef debug;

void debug_init(void)
{
  /* Configure GPIO pins */
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* RX pin */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT; //GPIO_MODE_AF_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* TX pin */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP; //GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  __HAL_RCC_USART1_FORCE_RESET();
  __HAL_RCC_USART1_RELEASE_RESET();
  __HAL_RCC_USART1_CLK_ENABLE();

  // R9M - UART on external pin stripe
  debug.Instance = USART1;
  debug.Init.BaudRate = 57600;
  debug.Init.WordLength = UART_WORDLENGTH_8B;
  debug.Init.StopBits = UART_STOPBITS_1;
  debug.Init.Parity = UART_PARITY_NONE;
  debug.Init.Mode = UART_MODE_TX;
  debug.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  debug.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&debug) != HAL_OK)
  {
    Error_Handler();
  }
}

void debug_send(uint8_t data)
{
  /* Make available the UART module. */
  if (HAL_UART_STATE_TIMEOUT == HAL_UART_GetState(&debug))
  {
    HAL_UART_Abort(&debug);
  }
  HAL_UART_Transmit(&debug, &data, 1u, UART_TIMEOUT);
}

#endif

/**
 * @brief   Receives data from UART.
 * @param   *data: Array to save the received data.
 * @param   length:  Size of the data.
 * @return  status: Report about the success of the receiving.
 */
uart_status uart_receive(uint8_t *data, uint16_t length)
{
  uart_status status = UART_ERROR;
#if HALF_DUPLEX
  HAL_HalfDuplex_EnableReceiver(&huart1);
#endif
  if (HAL_OK == HAL_UART_Receive(&huart1, data, length, UART_TIMEOUT))
  {
    status = UART_OK;
  }
  return status;
}

uart_status uart_receive_timeout(uint8_t *data, uint16_t length, uint16_t timeout)
{
  uart_status status = UART_ERROR;
#if HALF_DUPLEX
  HAL_HalfDuplex_EnableReceiver(&huart1);
#endif
  if (HAL_OK == HAL_UART_Receive(&huart1, data, length, timeout))
  {
    status = UART_OK;
  }
  return status;
}

/**
 * @brief   Transmits a string to UART.
 * @param   *data: Array of the data.
 * @return  status: Report about the success of the transmission.
 */
uart_status uart_transmit_str(uint8_t *data)
{
  uart_status status = UART_ERROR;
  uint16_t length = 0u;

  /* Calculate the length. */
  while ('\0' != data[length])
  {
    length++;
  }

  duplex_state_set(DUPLEX_TX);
#if HALF_DUPLEX
  HAL_HalfDuplex_EnableTransmitter(&huart1);
#endif
  if (HAL_OK == HAL_UART_Transmit(&huart1, data, length, UART_TIMEOUT))
  {
    status = UART_OK;
  }
  duplex_state_set(DUPLEX_RX);

  return status;
}

/**
 * @brief   Transmits a single char to UART.
 * @param   *data: The char.
 * @return  status: Report about the success of the transmission.
 */
uart_status uart_transmit_ch(uint8_t data)
{
  uart_status status = UART_ERROR;

  /* Make available the UART module. */
  if (HAL_UART_STATE_TIMEOUT == HAL_UART_GetState(&huart1))
  {
#ifndef STM32L1xx
    HAL_UART_Abort(&huart1);
#else
    return status;
#endif
  }
  duplex_state_set(DUPLEX_TX);
#if HALF_DUPLEX
  HAL_HalfDuplex_EnableTransmitter(&huart1);
#endif
  if (HAL_OK == HAL_UART_Transmit(&huart1, &data, 1u, UART_TIMEOUT))
  {
    status = UART_OK;
  }
  duplex_state_set(DUPLEX_RX);

  return status;
}

uart_status uart_transmit_bytes(uint8_t *data, uint32_t len)
{
  uart_status status = UART_ERROR;

  duplex_state_set(DUPLEX_TX);
#if HALF_DUPLEX
  HAL_HalfDuplex_EnableTransmitter(&huart1);
#endif
  if (HAL_OK == HAL_UART_Transmit(&huart1, data, len, UART_TIMEOUT))
  {
    status = UART_OK;
  }
  duplex_state_set(DUPLEX_RX);

  return status;
}

/**
 * @brief UART Initialization Function
 * @param None
 * @retval None
 */
void uart_init(void)
{
  memset(&huart1, 0, sizeof(huart1));

#ifdef DEBUG_UART
  debug_init();
#endif

#if UART_NUM == 1
  // R9M - UART on external pin stripe
  huart1.Instance = USART1;
#if AFIO_USART1_ENABLE
  GPIO_TypeDef *gpio_ptr = GPIOB;
  uint16_t pin_rx = 7;
  uint16_t pin_tx = 6;
#else
  GPIO_TypeDef *gpio_ptr = GPIOA;
  uint16_t pin_rx = 10;
  uint16_t pin_tx = 9;
#endif

#elif UART_NUM == 2
  // R9M - not used
  huart1.Instance = USART2;
  GPIO_TypeDef *gpio_ptr = GPIOA;
  uint16_t pin_rx = 3;
  uint16_t pin_tx = 2;

#elif UART_NUM == 3 && defined(USART3)
  // R9M - S.Port ?
  huart1.Instance = USART3;
  GPIO_TypeDef *gpio_ptr = GPIOB;
  uint16_t pin_rx = 11;
  uint16_t pin_tx = 10;

#else
#error "Invalid UART config"
#endif

  /* Configure GPIO pins */
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (huart1.Instance == USART1)
  {
    __HAL_RCC_USART1_FORCE_RESET();
    __HAL_RCC_USART1_RELEASE_RESET();
    __HAL_RCC_USART1_CLK_ENABLE();
  }
  else if (huart1.Instance == USART2)
  {
    __HAL_RCC_USART2_FORCE_RESET();
    __HAL_RCC_USART2_RELEASE_RESET();
    __HAL_RCC_USART2_CLK_ENABLE();
  }
#if defined(USART3)
  else if (huart1.Instance == USART3) {
    __HAL_RCC_USART3_FORCE_RESET();
    __HAL_RCC_USART3_RELEASE_RESET();
    __HAL_RCC_USART3_CLK_ENABLE();
  }
#endif

  /* RX pin */
  GPIO_InitStruct.Pin = (1 << pin_rx);
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
#ifdef STM32L0xx
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  if (huart1.Instance == USART1 && pin_rx == 7)
  {
    GPIO_InitStruct.Alternate = GPIO_AF0_USART1;
  }
  else
  {
    GPIO_InitStruct.Alternate = GPIO_AF4_USART1;
  }
#else
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
#endif
  HAL_GPIO_Init(gpio_ptr, &GPIO_InitStruct);

  /* TX pin */
  GPIO_InitStruct.Pin = (1 << pin_tx);
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
#ifdef STM32L0xx
  if (huart1.Instance == USART1 && pin_tx == 6) {
    GPIO_InitStruct.Alternate = GPIO_AF0_USART1;
  }
  else
  {
    GPIO_InitStruct.Alternate = GPIO_AF4_USART1;
  }
#endif
  HAL_GPIO_Init(gpio_ptr, &GPIO_InitStruct);

  duplex_state_set(DUPLEX_RX);

  huart1.Init.BaudRate = UART_BAUD;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
#if HALF_DUPLEX
  huart1.Init.Mode = UART_MODE_RX;
#else
  huart1.Init.Mode = UART_MODE_TX_RX;
#endif
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}
