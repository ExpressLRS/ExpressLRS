// STM32 serial
//
// Copyright (C) 2019  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "HardwareSerial.h"
#include "internal.h"
#include "irq.h"
#include "FIFO.h"

#define USART_BRR_DIV_Fraction_Pos (0U)
#define USART_BRR_DIV_Mantissa_Pos (4U)

#define DIV_ROUND_CLOSEST(x, divisor) ({       \
    typeof(divisor) __divisor = divisor;       \
    (((x) + ((__divisor) / 2)) / (__divisor)); \
})

static FIFO input;
static FIFO output;

#define CR1_FLAGS (USART_CR1_UE | USART_CR1_RE | USART_CR1_TE)

static USART_TypeDef *uart = NULL;

#ifdef __cplusplus
extern "C"
{
#endif
    void USARTx_IRQHandler(void)
    {
        uint32_t sr = uart->SR;

        if (sr & (USART_SR_RXNE | USART_SR_ORE))
        {
            input.push(uart->DR);
        }

        if ((sr & USART_SR_TXE) && (uart->CR1 & USART_CR1_TXEIE))
        {
            //uart->DR = 'T';
            if (output.size())
            {
                uart->DR = output.pop();
            }
            else
            {
                // No more TX data, enable RX
                //uart->CR1 = CR1_FLAGS | USART_CR1_RXNEIE;
            }
        }
    }
#ifdef __cplusplus
}
#endif

HardwareSerial::HardwareSerial(uint32_t rx, uint32_t tx)
{
    rx_pin = rx;
    tx_pin = tx;

    if (rx == GPIO('A', 10) && tx == GPIO('A', 9))
        uart = USART1;
    else if (rx == GPIO('A', 3) && tx == GPIO('A', 2))
        uart = USART2;
    else if ((rx == GPIO('D', 9) && tx == GPIO('D', 8)) ||
             (rx == GPIO('B', 11) && tx == GPIO('B', 10)))
        uart = USART3;
}

HardwareSerial::HardwareSerial(void *peripheral)
{
    uart = (USART_TypeDef *)peripheral;
    if (uart == USART1)
    {
        rx_pin = GPIO('A', 10);
        tx_pin = GPIO('A', 9);
    }
    else if (uart == USART2)
    {
        rx_pin = GPIO('A', 3);
        tx_pin = GPIO('A', 2);
    }
    else if (uart == USART3)
    {
        rx_pin = GPIO('B', 11);
        tx_pin = GPIO('B', 10);
    }
}

void HardwareSerial::begin(unsigned long baud, uint8_t mode)
{
    if (uart == USART1)
    {
        irq = USART1_IRQn;
    }
    else if (uart == USART2)
    {
        irq = USART2_IRQn;
    }
    else if (uart == USART3)
    {
        irq = USART3_IRQn;
    }
    enable_pclock((uint32_t)uart);

    uint32_t pclk = get_pclock_frequency((uint32_t)uart);
    uint32_t div = DIV_ROUND_CLOSEST(pclk, baud);
    uart->BRR = (((div / 16) << USART_BRR_DIV_Mantissa_Pos) | ((div % 16) << USART_BRR_DIV_Fraction_Pos));

    HAL_HalfDuplex_EnableReceiver(NULL);

    NVIC_SetPriority((IRQn_Type)irq, 1);
    NVIC_EnableIRQ((IRQn_Type)irq);

    gpio_peripheral(rx_pin, GPIO_FUNCTION(7), 1);
    gpio_peripheral(tx_pin, GPIO_FUNCTION(7), 0);
}

void HardwareSerial::end(void)
{
    uart->CR1 = 0;
    NVIC_DisableIRQ((IRQn_Type)irq);
}

int HardwareSerial::available(void)
{
    return input.size();
}

int HardwareSerial::peek(void)
{
    return input.peek();
}

int HardwareSerial::read(void)
{
    if (!available())
        return -1;
    irqstatus_t flag = irq_save();
    uint8_t data = input.pop();
    irq_restore(flag);
    return data;
}

void HardwareSerial::flush(void)
{
    // Wait until data is sent
    while (output.size())
        ;
}

uint32_t HardwareSerial::write(uint8_t data)
{
    irqstatus_t flag = irq_save();
    output.push(data);
    irq_restore(flag);
    return 1;
}

uint32_t HardwareSerial::write(const uint8_t *buff, uint32_t len)
{
    irqstatus_t flag = irq_save();
    output.pushBytes((uint8_t *)buff, len);
    irq_restore(flag);
    return len;
}

void HardwareSerial::HAL_HalfDuplex_EnableReceiver(void *)
{
    uart->CR1 = CR1_FLAGS | USART_CR1_RXNEIE;
}

void HardwareSerial::HAL_HalfDuplex_EnableTransmitter(void *)
{
    uart->CR1 = CR1_FLAGS | USART_CR1_TXEIE;
}
