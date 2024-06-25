/*
 * Copyright (C) Richard C. L. Li
 *
 * License GPLv3: https://www.gnu.org/licenses/gpl-3.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

#include <stdint.h>

#include "stm32f072xb.h"
#include "stm32f0xx_hal.h"
#include "stm32f0xx_hal_gpio.h"
#include "HardwareSerial.h"

#define USART_RX_BUFFER_SIZE 128
#define USART_TX_BUFFER_SIZE 128

extern "C" void SerialPortDMATxHandler(uint8_t SerialPortNo);

enum SerialPortParity
{
  Parity_None,
  Parity_Even,
  Parity_Odd,
};

enum SerialPortStopBits
{
  StopBits_One,
  StopBits_OneAndHalf,
  StopBits_Two,
};

enum SerialPortWordLength
{
  WordLength_8,
  WordLength_9,
};

typedef struct
{
  USART_TypeDef* usart;
  GPIO_TypeDef* gpioPort;
  uint16_t txPin;
  uint16_t rxPin;
  uint32_t alternate;
  IRQn_Type usartIRQ;
  DMA_TypeDef* txDMA;
  uint32_t txDMAChannel;
  IRQn_Type txDMAIRQ;
  DMA_TypeDef* rxDMA;
  uint32_t rxDMAChannel;
} USARTDef;

typedef struct
{
  uint32_t baud;       // = 0;
  uint8_t parity;      // = Parity_None;
  uint8_t wordLength;  // = WordLength_8;
  uint8_t stopBits;    // = StopBits_One;
} SerialPortParams;

class SerialPort: public Stream
{
  public:

  SerialPort(const USARTDef* def);
  void init(const SerialPortParams* params, bool invert);
  int available();
  int read();
  int peek();
  void flush();
  void end();
  void begin(int baud);
  void begin(int baud, bool invert);
  void setHalfDuplex(void);
  void enableHalfDuplexRx();
  int availableForWrite();

  size_t write(uint8_t byte);
  size_t write(const uint8_t* data, size_t len);
  using Print::write; // pull in write(str) from Print

  void setRx(uint32_t _rx) {}
  void setTx(uint32_t _tx) {}
  void setRx(PinName _rx) {}
  void setTx(PinName _tx) {}

  private:

  size_t add(uint8_t ch);
  void activateTxDMA();
  void handleInterrupt();
  bool LL_DMA_IsActiveFlag_TC();
  void LL_DMA_ClearFlag_TC();

  friend void SerialPortDMATxHandler(uint8_t SerialPortNo);

  const USARTDef* usartDef;
  uint8_t rxBuffer[USART_RX_BUFFER_SIZE];
  uint16_t rxHead = 0;
  uint16_t rxTail = 0;
  uint8_t txBuffer[USART_TX_BUFFER_SIZE];
  uint16_t txHead = 0;
  uint16_t txTail = 0;
  uint16_t txDMAWriteSize = 0;
  bool halfDuplexMode = false;

};

extern "C" SerialPort& getSerialPort(uint8_t SerialPortNo);
