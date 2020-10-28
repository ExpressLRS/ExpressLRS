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
#if TARGET_GHOST_RX_V1_2
static UART_HandleTypeDef huart_tx;
#define UART_TX_HANDLE huart_tx
#else
#define UART_TX_HANDLE huart1
#endif

#ifndef UART_BAUD
#define UART_BAUD 420000
#endif

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
  HAL_HalfDuplex_EnableTransmitter(&UART_TX_HANDLE);
#endif
  if (HAL_OK == HAL_UART_Transmit(&UART_TX_HANDLE, data, length, UART_TIMEOUT))
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
  if (HAL_UART_STATE_TIMEOUT == HAL_UART_GetState(&UART_TX_HANDLE))
  {
#ifndef STM32L1xx
    HAL_UART_Abort(&UART_TX_HANDLE);
#else
    return status;
#endif
  }
  duplex_state_set(DUPLEX_TX);
#if HALF_DUPLEX
  HAL_HalfDuplex_EnableTransmitter(&UART_TX_HANDLE);
#endif
  if (HAL_OK == HAL_UART_Transmit(&UART_TX_HANDLE, &data, 1u, UART_TIMEOUT))
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
  HAL_HalfDuplex_EnableTransmitter(&UART_TX_HANDLE);
#endif
  if (HAL_OK == HAL_UART_Transmit(&UART_TX_HANDLE, data, len, UART_TIMEOUT))
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
  /* Configure GPIO pins */
  GPIO_InitTypeDef GPIO_InitStruct;

  memset(&huart1, 0, sizeof(huart1));

#ifdef DEBUG_UART
  debug_init();
#endif

#if TARGET_GHOST_RX_V1_2
  // RX = PB6 [USART1]
  // TX = PA2 [USART2]

  /* Reset RX UART */
  __HAL_RCC_USART1_FORCE_RESET();
  __HAL_RCC_USART1_RELEASE_RESET();
  __HAL_RCC_USART1_CLK_ENABLE();

  /* Reset TX UART */
  __HAL_RCC_USART2_FORCE_RESET();
  __HAL_RCC_USART2_RELEASE_RESET();
  __HAL_RCC_USART2_CLK_ENABLE();

  /* Init RX pin */
  GPIO_InitStruct.Pin = (1 << 6);
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Init TX pin */
  GPIO_InitStruct.Pin = (1 << 2);
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Init TX UART */
  huart_tx.Instance = USART2;
  huart_tx.Init.BaudRate = UART_BAUD;
  huart_tx.Init.WordLength = UART_WORDLENGTH_8B;
  huart_tx.Init.StopBits = UART_STOPBITS_1;
  huart_tx.Init.Parity = UART_PARITY_NONE;
  huart_tx.Init.Mode = UART_MODE_TX;
  huart_tx.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart_tx.Init.OverSampling = UART_OVERSAMPLING_16;
  huart_tx.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  if (HAL_HalfDuplex_Init(&huart_tx) != HAL_OK) {
    Error_Handler();
  }

  /* Init RX UART */
  memcpy(&huart1.Init, &huart_tx.Init, sizeof(huart_tx.Init));
  huart1.Instance = USART1;
  huart1.Init.Mode = UART_MODE_RX;
  if (HAL_HalfDuplex_Init(&huart1) != HAL_OK) {
    Error_Handler();
  }

#else //!TARGET_GHOST_RX_V1_2

#if UART_NUM == 1
  huart1.Instance = USART1;
#if AFIO_USART1_ENABLE == 1
  GPIO_TypeDef *gpio_ptr = GPIOB;
  uint16_t pin_rx = 7;
  uint16_t pin_tx = 6;
#elif AFIO_USART1_ENABLE == 2
  GPIO_TypeDef *gpio_ptr = GPIOC;
  uint16_t pin_rx = 5;
  uint16_t pin_tx = 4;
#else
  GPIO_TypeDef *gpio_ptr = GPIOA;
  uint16_t pin_rx = 10;
  uint16_t pin_tx = 9;
#endif

#elif UART_NUM == 2
  huart1.Instance = USART2;
#if AFIO_USART2_ENABLE == 1
  /* JTAG pins. Need remapping! */
  GPIO_TypeDef *gpio_ptr = GPIOA;
  uint16_t pin_rx = 15;
  uint16_t pin_tx = 14;
#elif AFIO_USART2_ENABLE == 2
  /* JTAG pins. Need remapping! */
  GPIO_TypeDef *gpio_ptr = GPIOB;
  uint16_t pin_rx = 4;
  uint16_t pin_tx = 3;
#else //! AFIO_USART2_ENABLE
  GPIO_TypeDef *gpio_ptr = GPIOA;
  uint16_t pin_rx = 3;
  uint16_t pin_tx = 2;
#endif

#elif UART_NUM == 3 && defined(USART3)
  huart1.Instance = USART3;

#if AFIO_USART3_ENABLE == 1
  GPIO_TypeDef *gpio_ptr = GPIOB;
  uint16_t pin_rx = 8;
  uint16_t pin_tx = 9;
#elif AFIO_USART3_ENABLE == 2
  GPIO_TypeDef *gpio_ptr = GPIOC;
  uint16_t pin_rx = 11;
  uint16_t pin_tx = 10;
#else
  GPIO_TypeDef *gpio_ptr = GPIOB;
  uint16_t pin_rx = 11;
  uint16_t pin_tx = 10;
#endif

#else
#error "Invalid UART config"
#endif

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
#if defined(STM32L0xx)
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  if (huart1.Instance == USART1 && pin_rx == 7) {
    GPIO_InitStruct.Alternate = GPIO_AF0_USART1;
  } else {
    GPIO_InitStruct.Alternate = GPIO_AF4_USART1;
  }
#elif defined(STM32L4xx) || defined(STM32F3xx)
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
#else
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
#endif
  HAL_GPIO_Init(gpio_ptr, &GPIO_InitStruct);

  /* TX pin */
  GPIO_InitStruct.Pin = (1 << pin_tx);
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
#if defined(STM32L0xx)
  if (huart1.Instance == USART1 && pin_tx == 6) {
    GPIO_InitStruct.Alternate = GPIO_AF0_USART1;
  } else {
    GPIO_InitStruct.Alternate = GPIO_AF4_USART1;
  }
#elif defined(STM32L4xx) || defined(STM32F3xx)
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
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
#endif // TARGET_GHOST_RX_V1_2
}
