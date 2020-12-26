/**
 * @file    uart.h
 * @author  Ferenc Nemeth
 * @date    21 Dec 2018
 * @brief   This module is a layer between the HAL UART functions and my Xmodem protocol.
 *
 *          Copyright (c) 2018 Ferenc Nemeth - https://github.com/ferenc-nemeth
 */

#ifndef UART_H_
#define UART_H_

#include <stdint.h>

/* Timeout for HAL. */
#define UART_TIMEOUT ((uint16_t)2000u)

/* Status report for the functions. */
typedef enum
{
  UART_OK = 0x00u,   /**< The action was successful. */
  UART_ERROR = 0xFFu /**< Generic error. */
} uart_status;

uart_status uart_clear(void);
uart_status uart_receive(uint8_t *data, uint16_t length);
uart_status uart_receive_timeout(uint8_t *data, uint16_t length, uint16_t timeout);
uart_status uart_transmit_str(char * data);
uart_status uart_transmit_ch(uint8_t data);
uart_status uart_transmit_bytes(uint8_t *data, uint32_t len);

void uart_init(uint32_t baud, uint32_t rx_pin, uint32_t tx_pin, int32_t duplexpin);
void uart_deinit(void);

#endif /* UART_H_ */
