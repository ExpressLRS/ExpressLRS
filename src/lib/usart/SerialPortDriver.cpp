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

#include <stdio.h>
#include <stdarg.h>
#include "SerialPortDriver.h"
#include "stm32f0xx_ll_gpio.h"
#include "stm32f0xx_ll_usart.h"
#include "stm32f0xx_ll_dma.h"
#include "stm32f0xx_ll_bus.h"

static const USARTDef usartDef0 = {
  .usart = USART1,
  .gpioPort = GPIOA,
  .txPin = LL_GPIO_PIN_9,
  .rxPin = LL_GPIO_PIN_10,
  .alternate = LL_GPIO_AF_1,
  .usartIRQ = USART1_IRQn,
  .txDMA = DMA1,
  .txDMAChannel = LL_DMA_CHANNEL_2,
  .txDMAIRQ = DMA1_Channel2_3_IRQn,
  .rxDMA = DMA1,
  .rxDMAChannel = LL_DMA_CHANNEL_3
};

static const USARTDef usartDef1 = {
  .usart = USART2,
  .gpioPort = GPIOA,
  .txPin = LL_GPIO_PIN_2,
  .rxPin = LL_GPIO_PIN_3,
  .alternate = LL_GPIO_AF_1,
  .usartIRQ = USART2_IRQn,
  .txDMA = DMA1,
  .txDMAChannel = LL_DMA_CHANNEL_4,
  .txDMAIRQ = DMA1_Channel4_5_IRQn,
  .rxDMA = DMA1,
  .rxDMAChannel = LL_DMA_CHANNEL_5
};

static SerialPort SerialPortPorts[] = {
  SerialPort(&usartDef0),
  SerialPort(&usartDef1),
};

SerialPort::SerialPort(const USARTDef* def)
{
  usartDef = def;
}

void SerialPort::init(const SerialPortParams* params)
{
  // Enable USART clock
  if (usartDef->usart == USART1)
  {
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_USART1);
  }
  else if (usartDef->usart == USART2)
  {
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);
  }
  // Enable GPIO clock
  if (usartDef->gpioPort == GPIOA)
  {
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
  }
  else if (usartDef->gpioPort == GPIOB)
  {
    LL_APB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
  }
  else if (usartDef->gpioPort == GPIOC)
  {
    LL_APB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
  }

  // Enable DMA clock
  if (usartDef->txDMA == DMA1 || usartDef->rxDMA == DMA1 )
  {
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
  }

  // GPIO configure
  LL_GPIO_InitTypeDef gpioInitStruct;
  LL_GPIO_StructInit(&gpioInitStruct);
  gpioInitStruct.Pin = usartDef->txPin | usartDef->rxPin;
  gpioInitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  gpioInitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  gpioInitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  gpioInitStruct.Pull = LL_GPIO_PULL_UP;
  gpioInitStruct.Alternate = usartDef->alternate;
  LL_GPIO_Init(usartDef->gpioPort, &gpioInitStruct);

  // USART configure
  LL_USART_InitTypeDef usartInit = {
    .BaudRate = params->baud,
    .DataWidth = params->wordLength == WordLength_9 ? LL_USART_DATAWIDTH_9B : LL_USART_DATAWIDTH_8B,
    .StopBits = params->stopBits == StopBits_Two ? LL_USART_STOPBITS_2 : (params->stopBits == StopBits_OneAndHalf ? LL_USART_STOPBITS_1_5 : LL_USART_STOPBITS_1),
    .Parity = params->parity == Parity_Odd ? LL_USART_PARITY_ODD : (params->parity == Parity_Even ? LL_USART_PARITY_EVEN : LL_USART_PARITY_NONE),
    .TransferDirection = LL_USART_DIRECTION_TX_RX,
    .HardwareFlowControl = LL_USART_HWCONTROL_NONE,
    .OverSampling = LL_USART_OVERSAMPLING_8
  };
  LL_USART_DeInit(usartDef->usart);
  LL_USART_Init(usartDef->usart, &usartInit);

  LL_USART_DisableDMADeactOnRxErr(usartDef->usart);
  LL_USART_DisableHalfDuplex(usartDef->usart);
  LL_USART_DisableRxTimeout(usartDef->usart);
  
  // Enable USART
  LL_USART_Enable(usartDef->usart);

  // Disable IRQ based RX
  LL_USART_DisableIT_RXNE(usartDef->usart);
  NVIC_DisableIRQ(usartDef->usartIRQ);

  // Init RX DMA
  LL_DMA_InitTypeDef dmaInit = {
    .PeriphOrM2MSrcAddress = LL_USART_DMA_GetRegAddr(usartDef->usart, LL_USART_DMA_REG_DATA_RECEIVE),
    .MemoryOrM2MDstAddress = (uint32_t)rxBuffer,
    .Direction = LL_DMA_DIRECTION_PERIPH_TO_MEMORY,
    .Mode = LL_DMA_MODE_CIRCULAR,
    .PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT,
    .MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT,
    .PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE,
    .MemoryOrM2MDstDataSize = LL_DMA_PDATAALIGN_BYTE,
    .NbData = USART_RX_BUFFER_SIZE,
    .Priority = LL_DMA_PRIORITY_HIGH
  };
  LL_DMA_DeInit(usartDef->rxDMA, usartDef->rxDMAChannel);
  LL_DMA_Init(usartDef->rxDMA, usartDef->rxDMAChannel, &dmaInit);

  // Enable RX DMA channel
  LL_DMA_EnableChannel(usartDef->rxDMA, usartDef->rxDMAChannel);
  LL_USART_EnableDMAReq_RX(usartDef->usart);
}

int SerialPort::available()
{
  rxHead = USART_RX_BUFFER_SIZE - LL_DMA_GetDataLength(usartDef->rxDMA, usartDef->rxDMAChannel);
  if (rxHead < rxTail)
  {
    return USART_RX_BUFFER_SIZE + rxHead - rxTail;
  }
  else
  {
    return rxHead - rxTail;
  }
}

int SerialPort::read()
{
  if (available() > 0)
  {
    uint8_t data = rxBuffer[rxTail];
    rxTail = (rxTail + 1) % USART_RX_BUFFER_SIZE;
    return data;
  }
  else
  {
    return -1;
  }
}

int SerialPort::peek()
{
  if (available() > 0)
  {
    uint8_t data = rxBuffer[rxTail];
    return data;
  }
  else
  {
    return -1;
  }
}

void SerialPort::end()
{
  LL_DMA_DisableChannel(usartDef->rxDMA, usartDef->rxDMAChannel);
  LL_DMA_DisableChannel(usartDef->txDMA, usartDef->txDMAChannel);
  LL_USART_DisableDMAReq_RX(usartDef->usart);
  LL_USART_DisableDMAReq_TX(usartDef->usart);
  LL_USART_Disable(usartDef->usart);
}

void SerialPort::begin(int baud)
{
  SerialPortParams params = {
    .baud = (uint32_t) baud,
    .parity = Parity_None,
    .wordLength = WordLength_8,
    .stopBits = StopBits_One,
  };
  init(&params);
}

void SerialPort::enableHalfDuplexRx()
{

}

int SerialPort::availableForWrite()
{
  if (txHead < txTail)
  {
    return txTail - txHead - 1;
  }
  else
  {
    return USART_TX_BUFFER_SIZE + txTail - txHead - 1;
  }
}

size_t SerialPort::write(uint8_t byte)
{
  size_t size = add(byte);
  if (size > 0)
  {
    activateTxDMA();
  }
  return size;
}

size_t SerialPort::write(const uint8_t* data, size_t len)
{
  size_t count = 0;
  while (len > 0)
  {
    while (len > 0 && add(data[count]) > 0)
    {
      count++;
      len--;
    }
    activateTxDMA();
  }
  return count;
}

size_t SerialPort::add(uint8_t ch)
{
  if ((txHead + 1) % USART_TX_BUFFER_SIZE == txTail)
  {
    return 0;
  }
  else
  {
    txBuffer[txHead] = ch;
    txHead = (txHead + 1) % USART_TX_BUFFER_SIZE;
  }

  return 1;
}

void SerialPort::activateTxDMA()
{
  uint8_t* bufferPtr;
  uint16_t size;
  bufferPtr = txBuffer + txTail;
  if (txDMAWriteSize > 0 || txHead == txTail)
  {
    size = 0;
  }
  else if (txTail < txHead)
  {
    size = txHead - txTail;
    txDMAWriteSize = size;
  }
  else
  {
    size = USART_TX_BUFFER_SIZE - txTail;
    txDMAWriteSize = size;
  }

  if (size > 0)
  {
    // Init DMA
    LL_DMA_InitTypeDef dmaInit = {
      .PeriphOrM2MSrcAddress = LL_USART_DMA_GetRegAddr(usartDef->usart, LL_USART_DMA_REG_DATA_TRANSMIT),
      .MemoryOrM2MDstAddress = (uint32_t)bufferPtr,
      .Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH,
      .Mode = LL_DMA_MODE_NORMAL,
      .PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT,
      .MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT,
      .PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE,
      .MemoryOrM2MDstDataSize = LL_DMA_PDATAALIGN_BYTE,
      .NbData = size,
      .Priority = LL_DMA_PRIORITY_HIGH
    };
    LL_DMA_DeInit(usartDef->txDMA, usartDef->txDMAChannel);
    LL_DMA_Init(usartDef->txDMA, usartDef->txDMAChannel, &dmaInit);

    // Enable DMA transfer complete interrupt
    LL_DMA_EnableIT_TC(usartDef->txDMA, usartDef->txDMAChannel);
    NVIC_SetPriority(usartDef->txDMAIRQ, 0);
    NVIC_EnableIRQ(usartDef->txDMAIRQ);

    // Enable DMA channel
    LL_DMA_EnableChannel(usartDef->txDMA, usartDef->txDMAChannel);
    LL_USART_EnableDMAReq_TX(usartDef->usart);
  }
}

bool SerialPort::LL_DMA_IsActiveFlag_TC()
{
  switch(usartDef->txDMAChannel)
  {
    case LL_DMA_CHANNEL_2:
      return LL_DMA_IsActiveFlag_TC2(usartDef->txDMA);
    case LL_DMA_CHANNEL_3:
      return LL_DMA_IsActiveFlag_TC3(usartDef->txDMA);
    case LL_DMA_CHANNEL_4:
      return LL_DMA_IsActiveFlag_TC4(usartDef->txDMA);
    case LL_DMA_CHANNEL_5:
      return LL_DMA_IsActiveFlag_TC5(usartDef->txDMA);
    default:
      return false;
  }
}

void SerialPort::LL_DMA_ClearFlag_TC()
{
  switch(usartDef->txDMAChannel)
  {
    case LL_DMA_CHANNEL_2:
      LL_DMA_ClearFlag_TC2(usartDef->txDMA);
      break;
    case LL_DMA_CHANNEL_3:
      LL_DMA_ClearFlag_TC3(usartDef->txDMA);
      break;
    case LL_DMA_CHANNEL_4:
      LL_DMA_ClearFlag_TC4(usartDef->txDMA);
      break;
    case LL_DMA_CHANNEL_5:
      LL_DMA_ClearFlag_TC5(usartDef->txDMA);
      break;
  }
}

void SerialPort::handleInterrupt()
{
  if (LL_DMA_IsActiveFlag_TC())
  {
    LL_DMA_ClearFlag_TC();
    txTail = (txTail + txDMAWriteSize) % USART_TX_BUFFER_SIZE;
    txDMAWriteSize = 0;
    if (txHead == txTail)
    {
      // Buffer empty
      LL_DMA_DisableIT_TC(usartDef->txDMA, usartDef->txDMAChannel);
    }
    else
    {
      activateTxDMA();
    }
  }
}

extern "C" SerialPort& getSerialPort(uint8_t SerialPortNo)
{
  if (SerialPortNo < sizeof(SerialPortPorts))
  {
    return SerialPortPorts[SerialPortNo];
  }
  else
  {
    return SerialPortPorts[0];
  }
}

extern "C" void SerialPortDMATxHandler(uint8_t SerialPortNo)
{
  getSerialPort(SerialPortNo).handleInterrupt();
}

extern "C" void DMA1_Channel2_3_IRQHandler(void)
{
  SerialPortDMATxHandler(0);
}

extern "C" void DMA1_Channel4_5_IRQHandler(void)
{
  SerialPortDMATxHandler(1);
}
