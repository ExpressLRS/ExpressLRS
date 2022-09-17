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
#include "irq.h"
#include <string.h>

#ifndef USART_USE_RX_ISR
#define USART_USE_RX_ISR 1
#endif
#ifndef USART_USE_TX_ISR
#define USART_USE_TX_ISR 0
#endif

#if defined(STM32F1)
#define StatReg         SR
#else
#define StatReg         ISR
#endif
#ifndef USART_SR_RXNE
#define USART_SR_RXNE   USART_ISR_RXNE
#endif
#ifndef USART_SR_ORE
#define USART_SR_ORE    USART_ISR_ORE
#endif
#ifndef USART_SR_FE
#define USART_SR_FE     USART_ISR_FE
#endif
#ifndef USART_SR_NE
#define USART_SR_NE     USART_ISR_NE
#endif
#ifndef USART_SR_TXE
#define USART_SR_TXE    USART_ISR_TXE
#endif
#ifndef USART_SR_TC
#define USART_SR_TC     USART_ISR_TC
#endif

#define USART_RX_ISR  (USART_CR1_RXNEIE * USART_USE_RX_ISR)
#define USART_TX_ISR  (USART_CR1_TXEIE  * USART_USE_TX_ISR)

#define UART_FLAGS    (USART_CR1_UE)
#define UART_TX_FLAGS (USART_CR1_TE | USART_TX_ISR)
#define UART_RX_FLAGS (USART_CR1_RE | USART_RX_ISR)

#define UART_DR_TX    (UART_FLAGS | UART_TX_FLAGS)
#define UART_DR_RX    (UART_FLAGS | UART_RX_FLAGS)

#define RX_ISR_LST    (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE)


static USART_TypeDef *UART_handle_rx, *UART_handle_tx;
static uint32_t UART_CR_RX, UART_CR_TX;
static struct gpio_pin duplex_pin;

enum
{
  DUPLEX_RX,
  DUPLEX_TX,
};

void duplex_setup_pin(int32_t pin)
{
  duplex_pin.reg = NULL;
  if (pin < 0)
    return;
  duplex_pin = GPIO_Setup(pin, GPIO_OUTPUT, -1);
}

#ifndef DUPLEX_INVERTED
#define DUPLEX_INVERTED 0
#endif // DUPLEX_INVERTED

void duplex_state_set(const uint8_t state)
{
  USART_TypeDef *handle = UART_handle_tx;
  if (state == DUPLEX_TX) {
      handle->CR1 = UART_CR_TX;
  } else {
    if (duplex_pin.reg || !(UART_CR_RX & USART_CR1_TE))
      while (!LL_USART_IsActiveFlag_TC(handle))
        ;
    handle->CR1 = UART_CR_RX;
  }

  if (duplex_pin.reg) {
    GPIO_Write(duplex_pin, ((state == DUPLEX_TX) ^ DUPLEX_INVERTED));
  }
}

void usart_pin_config(uint32_t pin, uint8_t isrx)
{
#if defined(STM32L0xx)
  uint32_t fn = GPIO_FUNCTION(4);
  if (pin == GPIO('B', 6) || pin == GPIO('B', 7)) {
    // USART1 is AF0
    fn = GPIO_FUNCTION(0);
  }
#else
  uint32_t fn = GPIO_FUNCTION(7);
#endif
  GPIO_Setup(pin, fn, isrx);
}

// **************************************************

static uint8_t rx_head, rx_tail;
static uint8_t rx_buffer[256];

static uint8_t tx_head, tx_tail;
static uint8_t tx_buffer[256];

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

int tx_buffer_push(const uint8_t *buff, uint32_t len)
{
  uint_fast8_t tmax = read_u8(&tx_head), tpos = read_u8(&tx_tail);
  if (tpos >= tmax) {
    tpos = tmax = 0;
    write_u8(&tx_head, 0);
    write_u8(&tx_tail, 0);
  }
  if ((tmax + len) > sizeof(tx_buffer)) {
    if ((tmax + len - tpos) > sizeof(tx_buffer))
      // Not enough space for message
      return 0;
    // Disable TX irq and move buffer
    write_u8(&tx_head, 0); // this stops TX irqs

    tpos = read_u8(&tx_tail);
    tmax -= tpos;
    memmove(&tx_buffer[0], &tx_buffer[tpos], tmax);
    write_u8(&tx_tail, 0);
    write_u8(&tx_head, tmax);
  }

  memcpy(&tx_buffer[tmax], buff, len);
  write_u8(&tx_head, (tmax + len));

  duplex_state_set(DUPLEX_TX);
  return len;
}

void uart_flush(void)
{
  while (UART_handle_tx->CR1 & USART_TX_ISR)
    ;
}

// ***********************

void USARTx_IRQ_handler(USART_TypeDef * uart)
{
  uint32_t CR = uart->CR1;
  uint32_t SR = uart->StatReg;
  /* Check for RX data */
  if (SR & RX_ISR_LST) {
    // Always read even if there is an error bit set, as this resets the error bits
    uint8_t data = (uint8_t)LL_USART_ReceiveData8(uart);
    // If RX is in interrupt mode, the RX not-empty bit is set, and the received byte
    // has no framing errors (framing errors still generate RXNE), add to the RX fifo
    if ((CR & USART_CR1_RXNEIE) && (SR & USART_SR_RXNE) && !(SR & USART_SR_FE)) {
      uint8_t next = rx_head;
      if ((next + 1) != rx_tail) {
        rx_buffer[rx_head++] = data;
        rx_head = next + 1;
      }
    }
  }

  // If TX is in interrupt mode, and TX empty bit set
  if ((CR & USART_CR1_TXEIE) && (SR & USART_SR_TXE)) {
    //  Check if data in TX fifo still to send, else switch to RX mode
    if (tx_head <= tx_tail)
      duplex_state_set(DUPLEX_RX);
    else
      LL_USART_TransmitData8(uart, tx_buffer[tx_tail++]);
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

// **************************************************

uart_status uart_clear(void)
{
  USART_TypeDef *handle = UART_handle_rx;

  if (!(UART_CR_RX & USART_CR1_TE)) {
    // Wait until TX is rdy
    uart_flush();
  }

  irqstatus_t irq = irq_save();
  write_u8(&rx_head, 0);
  write_u8(&rx_tail, 0);
  LL_USART_ClearFlag_ORE(handle);
#if !defined(STM32F1)
  LL_USART_ClearFlag_NE(handle);
  LL_USART_ClearFlag_FE(handle);
#endif
  irq_restore(irq);
  return UART_OK;
}

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
  uint32_t tickstart, SR;

  USART_TypeDef *handle = UART_handle_rx;
  if (!handle)
    return UART_ERROR;
  if (handle->CR1 & USART_CR1_RXNEIE) {
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
  } else {
    uint8_t rcvd;
    LL_USART_ClearFlag_ORE(handle);
#if !defined(STM32F1)
    LL_USART_ClearFlag_NE(handle);
    LL_USART_ClearFlag_FE(handle);
#endif

    while (length--) {
      tickstart = HAL_GetTick();
      do {
        SR = handle->StatReg;
        /* Check for the Timeout */
        if (timeout != HAL_MAX_DELAY) {
          if ((timeout == 0U) || ((HAL_GetTick() - tickstart) > timeout)) {
            return UART_ERROR;
          }
        }
      } while(!(SR & RX_ISR_LST));
      // Read RX register will clear also faults
      rcvd = (uint8_t)LL_USART_ReceiveData8(handle);
      if (SR & USART_SR_RXNE) {
        // Store only if data is valid
        *data++ = rcvd;
      }
    }
  }
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
  USART_TypeDef *handle = UART_handle_tx;
  if (!handle)
    return UART_ERROR;
  if (UART_CR_TX & USART_CR1_TXEIE) {
    if (!tx_buffer_push(data, len))
      status = UART_ERROR;
  } else {
    duplex_state_set(DUPLEX_TX);
    while (len--) {
      while (!LL_USART_IsActiveFlag_TXE(handle))
        ;
      LL_USART_TransmitData8(handle, *data++);
    }
    while (!LL_USART_IsActiveFlag_TXE(handle))
      ;
    duplex_state_set(DUPLEX_RX);
  }
  return status;
}

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

static void uart_reset(USART_TypeDef * uart_ptr)
{
  if (!uart_ptr) return;

  uart_ptr->CR1 = 0;

  IRQn_Type irq = usart_get_irq(uart_ptr);
  NVIC_DisableIRQ(irq);

  LL_USART_DisableDMAReq_RX(uart_ptr);
  LL_USART_DisableDMAReq_TX(uart_ptr);

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

static void usart_hw_init(USART_TypeDef *USARTx, uint32_t baud, uint32_t flags, uint8_t halfduplex) {
  uint32_t pclk = SystemCoreClock / 2;

  /* Reset UART peripheral */
  uart_reset(USARTx);

#if defined(STM32F1)
  LL_USART_SetBaudRate(USARTx, pclk, baud);
#else
  LL_USART_SetBaudRate(USARTx, pclk, LL_USART_OVERSAMPLING_16, baud);
#endif
  LL_USART_ConfigAsyncMode(USARTx);
  if (halfduplex)
    LL_USART_EnableHalfDuplex(USARTx);
  USARTx->CR1 = flags;
}

static uint8_t uart_valid_pin_tx(int32_t pin, uint8_t * swapped)
{
  switch (pin) {
#if defined(STM32F3xx)
    case GPIO('A', 3):
    case GPIO('A', 10):
    case GPIO('A', 15):
    case GPIO('B', 4):
    case GPIO('B', 7):
    case GPIO('B', 8):
    case GPIO('B', 11):
    case GPIO('C', 5):
    case GPIO('C', 11):
      if (swapped)
        *swapped = 1;
      return 1;
#endif
    case GPIO('A', 2):
    case GPIO('A', 9):
    case GPIO('A', 14):
    case GPIO('B', 3):
    case GPIO('B', 6):
    case GPIO('B', 9):
    case GPIO('B', 10):
    case GPIO('C', 4):
    case GPIO('C', 10):
      return 1;
  }
  return 0;
}

static USART_TypeDef * uart_peripheral_get(int32_t pin)
{
  switch (pin) {
    case GPIO('A', 9):
    case GPIO('A', 10):
    case GPIO('B', 6):
    case GPIO('B', 7):
    case GPIO('C', 4):
    case GPIO('C', 5):
      return USART1;
    case GPIO('A', 2):
    case GPIO('A', 3):
    case GPIO('A', 14):
    case GPIO('A', 15):
    case GPIO('B', 3):
    case GPIO('B', 4):
      return USART2;
#ifdef USART3
    case GPIO('B', 8):
    case GPIO('B', 9):
    case GPIO('B', 10):
    case GPIO('B', 11):
    case GPIO('C', 10):
    case GPIO('C', 11):
      return USART3;
#endif // USART3
  }
  return NULL;
}


/**
 * @brief UART Initialization Function
 * @param None
 * @retval None
 */
void uart_init(uint32_t baud, int32_t pin_rx, int32_t pin_tx,
               int32_t duplexpin, int8_t inverted)
{
  USART_TypeDef * uart_ptr = uart_peripheral_get(pin_tx);
  USART_TypeDef * uart_ptr_rx = uart_ptr;
  uint8_t halfduplex, swapped = 0;

  if (!uart_ptr || !uart_valid_pin_tx(pin_tx, &swapped)) {
    Error_Handler();
  }

  // No RX pin or same as TX pin == half duplex
  halfduplex = ((pin_rx < 0) || (pin_rx == pin_tx));

  UART_CR_RX = UART_DR_RX;
  UART_CR_TX = UART_DR_TX;

  if (0 <= pin_rx) {
    uart_ptr_rx = uart_peripheral_get(pin_rx);
    if (uart_ptr_rx && uart_ptr_rx != uart_ptr) {
      /* RX USART peripheral is not same as TX USART */
      usart_hw_init(uart_ptr_rx, baud, UART_CR_RX,
                    uart_valid_pin_tx(pin_rx, NULL));
      /* Set TX to half duplex and always enabled */
      UART_CR_RX = UART_FLAGS | USART_CR1_TE;
    } else {
      // full duplex
      uart_ptr_rx = uart_ptr;
      if (duplexpin < 0) {
        UART_CR_RX |= USART_CR1_TE;
        UART_CR_TX |= USART_CR1_RE;
      }
    }
  }

  /* Store handles */
  UART_handle_tx = uart_ptr;
  UART_handle_rx = uart_ptr_rx;

  /* TX USART peripheral config */
  usart_hw_init(uart_ptr, baud, 0, halfduplex);

#if defined(STM32F3xx) || defined(STM32F0)
  /* F3 can swap Rx and Tx pins */
  if (swapped) {
    LL_USART_SetTXRXSwap(uart_ptr, LL_USART_TXRX_SWAPPED);
    if (uart_ptr_rx != uart_ptr)
      LL_USART_SetTXRXSwap(uart_ptr_rx, LL_USART_TXRX_SWAPPED);
  }
  /* F3 can invert uart lines */
  if (inverted) {
    LL_USART_SetTXPinLevel(uart_ptr, LL_USART_TXPIN_LEVEL_INVERTED);
    LL_USART_SetRXPinLevel(uart_ptr_rx, LL_USART_RXPIN_LEVEL_INVERTED);
  }
#else
  (void)inverted;
#endif

  /* Duplex pin */
  duplex_setup_pin(duplexpin);
  /* Enable RX by default */
  duplex_state_set(DUPLEX_RX);

  IRQn_Type irq = usart_get_irq(uart_ptr_rx);
  if (irq) {
#if defined(STM32F030x8)
    NVIC_SetPriority(irq,2);
#else
    NVIC_SetPriority(irq,
        NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 2, 0));
#endif
    NVIC_EnableIRQ(irq);
  }
  if (uart_ptr_rx != uart_ptr) {
    irq = usart_get_irq(uart_ptr);
    if (irq) {
#if defined(STM32F030x8)
    NVIC_SetPriority(irq,2);
#else
      NVIC_SetPriority(irq,
          NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 2, 0));
#endif
      NVIC_EnableIRQ(irq);
    }
 }

  /* TX pin */
  usart_pin_config(pin_tx, (halfduplex ^ inverted));
  /* RX pin */
  if (0 <= pin_rx)
    usart_pin_config(pin_rx, !inverted);
}

void uart_deinit(void)
{
  if (UART_handle_rx)
    uart_reset(UART_handle_rx);
  if (UART_handle_rx != UART_handle_tx)
    uart_reset(UART_handle_tx);
}
