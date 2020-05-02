#include "HwSerial.h"
#include "targets.h"

#ifndef CRSF_SERIAL_NBR
#define CRSF_SERIAL_NBR 1
#elif (CRSF_SERIAL_NBR > 2)
#error "Not supported serial!"
#endif

#if CRSF_SERIAL_NBR == 0
#define UART_RXD_IDX (U0RXD_IN_IDX)
#define UART_TXD_IDX (U0TXD_OUT_IDX)
#elif CRSF_SERIAL_NBR == 1
#define UART_RXD_IDX (U1RXD_IN_IDX)
#define UART_TXD_IDX (U1TXD_OUT_IDX)
#elif CRSF_SERIAL_NBR == 2
#define UART_RXD_IDX (U2RXD_IN_IDX)
#define UART_TXD_IDX (U2TXD_OUT_IDX)
#endif

/********************************************************************************
 *                                   PUBLIC
 ********************************************************************************/
HwSerial CrsfSerial(CRSF_SERIAL_NBR, -1);

HwSerial::HwSerial(int uart_nr, int32_t pin) : HardwareSerial(uart_nr)
{
    duplex_pin = pin;
}

void HwSerial::Begin(uint32_t baud, uint32_t config)
{
    HardwareSerial::begin(baud, config, GPIO_PIN_RCSIGNAL_RX, GPIO_PIN_RCSIGNAL_TX, true);
    enable_receiver();
}

void HwSerial::enable_receiver(void)
{
    yield();
    HardwareSerial::flush(); // wait until write ends
    /* Detach TX pin */
    gpio_matrix_out((gpio_num_t)-1, UART_TXD_IDX, true, false);
    /* Attach RX pin */
    gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, GPIO_MODE_INPUT);
    gpio_matrix_in((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, UART_RXD_IDX, true);
    yield();
}

void HwSerial::enable_transmitter(void)
{
    delayMicroseconds(20);
    /* Detach RX pin */
    gpio_matrix_in((gpio_num_t)-1, UART_RXD_IDX, false);
    /* Attach TX pin */
    gpio_set_level((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, 0);
    gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, GPIO_MODE_OUTPUT);
    gpio_matrix_out((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, UART_TXD_IDX, true, false);
    yield();
}
