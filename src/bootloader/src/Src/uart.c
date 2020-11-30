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

#if defined(STM32L4xx)
// FIXME: UART ISR causes xmodem failure
#define USART_USE_RX_ISR 0
#else
#define USART_USE_RX_ISR 1
#endif

USART_TypeDef *UART_handle;
#if TARGET_GHOST_RX_V1_2 || TARGET_R9SLIM_PLUS
USART_TypeDef *UART_handle_tx;
#define UART_TX_HANDLE UART_handle_tx
#else
#define UART_TX_HANDLE UART_handle
#endif

#if USART_USE_RX_ISR
#define USART_CR1_FLAGS (USART_CR1_UE | LL_USART_CR1_RXNEIE)
#else
#define USART_CR1_FLAGS (USART_CR1_UE)
#endif

enum
{
  DUPLEX_RX,
  DUPLEX_TX,
};

static uint8_t uart_half_duplex;
static void *duplex_port;
static uint32_t duplex_pin;

void duplex_setup_pin(int32_t pin)
{
  duplex_port = NULL;
  if (pin < 0)
    return;

  gpio_port_pin_get(pin, &duplex_port, &duplex_pin);
  gpio_port_clock((uint32_t)duplex_port);
  LL_GPIO_SetPinMode(duplex_port, duplex_pin, LL_GPIO_MODE_OUTPUT);
  LL_GPIO_SetPinOutputType(duplex_port, duplex_pin, LL_GPIO_OUTPUT_PUSHPULL);
  LL_GPIO_SetPinSpeed(duplex_port, duplex_pin, LL_GPIO_SPEED_FREQ_LOW);
  LL_GPIO_ResetOutputPin(duplex_port, duplex_pin);
}

#ifndef DUPLEX_INVERTED
#define DUPLEX_INVERTED 0
#endif // DUPLEX_INVERTED

void duplex_state_set(const uint8_t state)
{
  if (uart_half_duplex) {
    switch (state) {
      case DUPLEX_TX:
        UART_TX_HANDLE->CR1 = USART_CR1_FLAGS | USART_CR1_TE;
        break;
      case DUPLEX_RX:
        UART_TX_HANDLE->CR1 = USART_CR1_FLAGS | USART_CR1_RE;
        break;
      default:
        break;
    }
  }
  if (duplex_port) {
    GPIO_WritePin(duplex_port, duplex_pin, ((state == DUPLEX_TX) ^ DUPLEX_INVERTED));
  }
}

void usart_rx_pin_config(GPIO_TypeDef *gpio_ptr, uint32_t pin_rx)
{
  /* RX pin */
#if defined(STM32L0xx)
  LL_GPIO_SetPinMode(gpio_ptr, pin_rx, LL_GPIO_MODE_ALTERNATE);
  if (gpio_ptr == GPIOB && pin_rx == LL_GPIO_PIN_7) {
    // USART1 is AF0
    LL_GPIO_SetAFPin_0_7(gpio_ptr, pin_rx, LL_GPIO_AF_0);
  } else if (pin_rx <= LL_GPIO_PIN_7) {
    LL_GPIO_SetAFPin_0_7(gpio_ptr, pin_rx, LL_GPIO_AF_4);
  } else {
    LL_GPIO_SetAFPin_8_15(gpio_ptr, pin_rx, LL_GPIO_AF_4);
  }
#elif defined(STM32L4xx) || defined(STM32F3xx)
  LL_GPIO_SetPinMode(gpio_ptr, pin_rx, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetPinSpeed(gpio_ptr, pin_rx, LL_GPIO_SPEED_FREQ_HIGH);
  if (pin_rx <= LL_GPIO_PIN_7) {
    LL_GPIO_SetAFPin_0_7(gpio_ptr, pin_rx, LL_GPIO_AF_7);
  } else {
    LL_GPIO_SetAFPin_8_15(gpio_ptr, pin_rx, LL_GPIO_AF_7);
  }
#else
  LL_GPIO_SetPinMode(gpio_ptr, pin_rx, LL_GPIO_MODE_INPUT);
#endif
  //LL_GPIO_SetPinSpeed(gpio_ptr, pin_rx, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinPull(gpio_ptr, pin_rx, LL_GPIO_PULL_UP);
}

void usart_tx_pin_config(GPIO_TypeDef *gpio_ptr, uint32_t pin_tx)
{
  /* TX pin */
  LL_GPIO_SetPinMode(gpio_ptr, pin_tx, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetPinSpeed(gpio_ptr, pin_tx, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinPull(gpio_ptr, pin_tx, LL_GPIO_PULL_UP);
#if defined(STM32L0xx)
  if (gpio_ptr == GPIOB && pin_tx == LL_GPIO_PIN_6) {
    // USART1 is AF0
    LL_GPIO_SetAFPin_0_7(gpio_ptr, pin_tx, LL_GPIO_AF_0);
  } else if (pin_tx <= LL_GPIO_PIN_7) {
    LL_GPIO_SetAFPin_0_7(gpio_ptr, pin_tx, LL_GPIO_AF_4);
  } else {
    LL_GPIO_SetAFPin_8_15(gpio_ptr, pin_tx, LL_GPIO_AF_4);
  }
#elif defined(STM32L4xx) || defined(STM32F3xx)
  if (pin_tx <= LL_GPIO_PIN_7) {
    LL_GPIO_SetAFPin_0_7(gpio_ptr, pin_tx, LL_GPIO_AF_7);
  } else {
    LL_GPIO_SetAFPin_8_15(gpio_ptr, pin_tx, LL_GPIO_AF_7);
  }
#endif
}

// **************************************************
#if USART_USE_RX_ISR

uint8_t rx_head, rx_tail;
uint8_t rx_buffer[256];

int rx_buffer_available(void)
{
    uint8_t head = read_u8(&rx_head), tail = read_u8(&rx_tail);
    return (uint8_t)(head - tail);
}

int rx_buffer_read(void)
{
    if (!rx_buffer_available())
        return -1;
    uint8_t tail = read_u8(&rx_tail);
    write_u8(&rx_tail, tail+1);
    return rx_buffer[tail++];
}

// ***********************

void USARTx_IRQ_handler(USART_TypeDef * uart)
{
  /* Check for RX data */
  if (LL_USART_IsActiveFlag_ORE(uart) || LL_USART_IsActiveFlag_RXNE(uart)) {
    uint8_t next = rx_head;
    uint8_t data = (uint8_t)LL_USART_ReceiveData8(uart);
    if ((next + 1) != rx_tail) {
      rx_buffer[next] = data;
      rx_head = next + 1;
    }
  }
}


void USART1_IRQHandler(void)
{
  USARTx_IRQ_handler(USART1);
}
void USART2_IRQHandler(void)
{
  USARTx_IRQ_handler(USART2);
}
#if defined(USART3)
void USART3_IRQHandler(void)
{
  USARTx_IRQ_handler(USART3);
}
#endif

#endif // USART_USE_RX_ISR

// **************************************************


/**
 * @brief   Receives data from UART.
 * @param   *data: Array to save the received data.
 * @param   length:  Size of the data.
 * @return  status: Report about the success of the receiving.
 */
uart_status uart_receive(uint8_t *data, uint16_t length)
{
  return uart_receive_timeout(data, length, UART_TIMEOUT);
}

uart_status uart_receive_timeout(uint8_t *data, uint16_t length, uint16_t timeout)
{
  uint32_t tickstart;
#if !USART_USE_RX_ISR
  LL_USART_ClearFlag_ORE(UART_handle);
  LL_USART_ClearFlag_NE(UART_handle);
  LL_USART_ClearFlag_FE(UART_handle);

  while (length--) {
    tickstart = HAL_GetTick();
    while (!LL_USART_IsActiveFlag_RXNE(UART_handle)) {
      /* Check for the Timeout */
      if (timeout != HAL_MAX_DELAY) {
        if ((timeout == 0U) || ((HAL_GetTick() - tickstart) > timeout)) {
          return UART_ERROR;
        }
      }
    }
    *data++ = (uint8_t)LL_USART_ReceiveData8(UART_handle);
  }
#else
  while (length--) {
    tickstart = HAL_GetTick();
    while (!rx_buffer_available()) {
      /* Check for the Timeout */
      if (timeout != HAL_MAX_DELAY) {
        if ((timeout == 0U) || ((HAL_GetTick() - tickstart) > timeout)) {
          return UART_ERROR;
        }
      }
    }
    *data++ = (uint8_t)rx_buffer_read();
  }
#endif

  return UART_OK;
}

/**
 * @brief   Transmits a string to UART.
 * @param   *data: Array of the data.
 * @return  status: Report about the success of the transmission.
 */
uart_status uart_transmit_str(char *data)
{
  uint32_t length = 0u;
  /* Calculate the length. */
  while ('\0' != data[length]) {
    length++;
  }
  return uart_transmit_bytes((uint8_t *)data, length);
}

/**
 * @brief   Transmits a single char to UART.
 * @param   *data: The char.
 * @return  status: Report about the success of the transmission.
 */
uart_status uart_transmit_ch(uint8_t data)
{
  return uart_transmit_bytes(&data, 1u);
}

uart_status uart_transmit_bytes(uint8_t *data, uint32_t len)
{
  uart_status status = UART_OK;
  duplex_state_set(DUPLEX_TX);
  while (len--) {
    LL_USART_TransmitData8(UART_TX_HANDLE, *data++);
    while (!LL_USART_IsActiveFlag_TXE(UART_TX_HANDLE))
      ;
  }
  while (!LL_USART_IsActiveFlag_TC(UART_TX_HANDLE))
    ;
  duplex_state_set(DUPLEX_RX);
  return status;
}

#if USART_USE_RX_ISR
static IRQn_Type usart_get_irq(USART_TypeDef *USARTx)
{
  if (USARTx == USART1) {
    return USART1_IRQn;
  } else if (USARTx == USART2) {
    return USART2_IRQn;
#if defined(USART3)
  } else if (USARTx == USART3) {
    return USART3_IRQn;
#endif // USART3
  }
  return 0;
}
#endif

static void usart_hw_init(USART_TypeDef *USARTx, uint32_t baud, uint32_t dir, uint8_t halfduplex) {
  uint32_t pclk = SystemCoreClock / 2;
#if defined(STM32F1)
  LL_USART_SetBaudRate(USARTx, pclk, baud);
#else
  LL_USART_SetBaudRate(USARTx, pclk, LL_USART_OVERSAMPLING_16, baud);
#endif
  LL_USART_ConfigAsyncMode(USARTx);
  if (halfduplex)
    LL_USART_EnableHalfDuplex(USARTx);
  USARTx->CR1 = USART_CR1_FLAGS | dir;
  uart_half_duplex = halfduplex;

#if USART_USE_RX_ISR
  IRQn_Type irq = usart_get_irq(USARTx);
  NVIC_SetPriority(irq,
      NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 2, 0));
  NVIC_EnableIRQ(irq);
#endif // USART_USE_RX_ISR
}

static void uart_reset(USART_TypeDef * uart_ptr)
{
  if (!uart_ptr) return;

  uart_ptr->CR1 = 0;

#if USART_USE_RX_ISR
  IRQn_Type irq = usart_get_irq(uart_ptr);
  NVIC_DisableIRQ(irq);
#endif

  if (uart_ptr == USART1) {
    __HAL_RCC_USART1_FORCE_RESET();
    __HAL_RCC_USART1_RELEASE_RESET();
    __HAL_RCC_USART1_CLK_ENABLE();
  } else if (uart_ptr == USART2) {
    __HAL_RCC_USART2_FORCE_RESET();
    __HAL_RCC_USART2_RELEASE_RESET();
    __HAL_RCC_USART2_CLK_ENABLE();
#if defined(USART3)
  } else if (uart_ptr == USART3) {
    __HAL_RCC_USART3_FORCE_RESET();
    __HAL_RCC_USART3_RELEASE_RESET();
    __HAL_RCC_USART3_CLK_ENABLE();
#endif
  }
}

/**
 * @brief UART Initialization Function
 * @param None
 * @retval None
 */
void uart_init(uint32_t baud, uint32_t uart_idx, uint32_t afio, int32_t duplexpin, uint8_t halfduplex)
{
#if TARGET_GHOST_RX_V1_2
  (void)duplexpin;
  (void)halfduplex;
  // RX = PB6 [USART1]
  // TX = PA2 [USART2]

  /* Reset RX UART */
  uart_reset(USART1);
  /* Reset TX UART */
  uart_reset(USART2);

  /* UART RX pin config */
  gpio_port_clock((uint32_t)GPIOB);
  usart_rx_pin_config(GPIOB, LL_GPIO_PIN_6);

  /* UART TX pin config */
  gpio_port_clock((uint32_t)GPIOA);
  usart_tx_pin_config(GPIOA, LL_GPIO_PIN_2);

  usart_hw_init(USART2, baud, USART_CR1_TE, 1); // TX, half duplex
  UART_TX_HANDLE = USART2;
  usart_hw_init(USART1, baud, USART_CR1_RE, 1); // RX, half duplex
  UART_handle = USART1;

#elif TARGET_R9SLIM_PLUS // !TARGET_GHOST_RX_V1_2
  (void)duplexpin;
  (void)halfduplex;

  // RX = PB11 [USART3]
  // TX = PA9 [USART1]

  /* Reset RX UART */
  uart_reset(USART3);
  /* Reset TX UART */
  uart_reset(USART1);

  /* UART RX pin config */
  gpio_port_clock((uint32_t)GPIOB);
  usart_rx_pin_config(GPIOB, LL_GPIO_PIN_11);

  /* UART TX pin config */
  gpio_port_clock((uint32_t)GPIOA);
  usart_tx_pin_config(GPIOA, LL_GPIO_PIN_9);

  usart_hw_init(USART1, baud, USART_CR1_TE, 0); // TX, half duplex
  UART_TX_HANDLE = USART1;
  usart_hw_init(USART3, baud, USART_CR1_RE, 0); // RX, half duplex
  UART_handle = USART3;

#else //!TARGET_GHOST_RX_V1_2 && !TARGET_R9SLIM_PLUS

  USART_TypeDef * uart_ptr;
  GPIO_TypeDef *gpio_ptr;
  uint32_t pin_rx, pin_tx, dir = USART_CR1_TE;

  switch (uart_idx) {
    case 1: {
      uart_ptr = USART1;
      switch (afio) {
        case 1:
          gpio_ptr = GPIOB;
          pin_rx = LL_GPIO_PIN_7;
          pin_tx = LL_GPIO_PIN_6;
          break;
        case 2:
          gpio_ptr = GPIOC;
          pin_rx = LL_GPIO_PIN_5;
          pin_tx = LL_GPIO_PIN_4;
          break;
        default:
          gpio_ptr = GPIOA;
          pin_rx = LL_GPIO_PIN_10;
          pin_tx = LL_GPIO_PIN_9;
          break;
      }
      break;
    }

    case 2: {
      uart_ptr = USART2;
      switch (afio) {
        case 1:
          /* JTAG pins. Need remapping! */
          gpio_ptr = GPIOA;
          pin_rx = LL_GPIO_PIN_15;
          pin_tx = LL_GPIO_PIN_14;
          break;
        case 2:
          /* JTAG pins. Need remapping! */
          gpio_ptr = GPIOB;
          pin_rx = LL_GPIO_PIN_4;
          pin_tx = LL_GPIO_PIN_3;
          break;
        default:
          gpio_ptr = GPIOA;
          pin_rx = LL_GPIO_PIN_3;
          pin_tx = LL_GPIO_PIN_2;
          break;
      }
      break;
    }

#if defined(USART3)
    case 3: {
      uart_ptr = USART3;
      switch (afio) {
        case 1:
          gpio_ptr = GPIOB;
          pin_rx = LL_GPIO_PIN_8;
          pin_tx = LL_GPIO_PIN_9;
          break;
        case 2:
          gpio_ptr = GPIOC;
          pin_rx = LL_GPIO_PIN_11;
          pin_tx = LL_GPIO_PIN_10;
          break;
        default:
          gpio_ptr = GPIOB;
          pin_rx = LL_GPIO_PIN_11;
          pin_tx = LL_GPIO_PIN_10;
          break;
      }
      break;
    }
#endif // defined(USART3)

    default:
      Error_Handler();
      return;
  }

  uart_reset(uart_ptr);
  gpio_port_clock((uint32_t)gpio_ptr);

  if (!halfduplex) {
    dir = USART_CR1_RE | USART_CR1_TE;
    /* RX pin */
    usart_rx_pin_config(gpio_ptr, pin_rx);
  }
  /* TX pin */
  usart_tx_pin_config(gpio_ptr, pin_tx);
  /* Duplex pin */
  duplex_setup_pin(duplexpin);
  duplex_state_set(DUPLEX_RX);
  /* Usart peripheral config */
  UART_handle = uart_ptr;
  usart_hw_init(uart_ptr, baud, dir, halfduplex);
#endif // TARGET_GHOST_RX_V1_2
}

void uart_deinit(void)
{
  if (UART_handle)
    uart_reset(UART_handle);
#if TARGET_GHOST_RX_V1_2 || TARGET_R9SLIM_PLUS
  if (UART_TX_HANDLE && UART_handle != UART_TX_HANDLE)
    uart_reset(UART_TX_HANDLE);
#endif
}
