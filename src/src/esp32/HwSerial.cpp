#include "HwSerial.h"
#include "targets.h"

#ifndef CRSF_SERIAL_NBR
#define CRSF_SERIAL_NBR 1
#elif (CRSF_SERIAL_NBR > 2)
#error "Not supported serial!"
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
    HardwareSerial::begin(baud, config);
    enable_receiver();
}

void HwSerial::enable_receiver(void)
{
    yield();
    HardwareSerial::flush(); // wait until write ends
    ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, GPIO_MODE_INPUT));
    //ESP_ERROR_CHECK(gpio_set_pull_mode((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, GPIO_FLOATING));
    //ESP_ERROR_CHECK(gpio_set_pull_mode(port->config.rx, port->config.inverted ? GPIO_PULLDOWN_ONLY : GPIO_PULLUP_ONLY));
    gpio_matrix_in((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, U1RXD_IN_IDX, true);
}

void HwSerial::enable_transmitter(void)
{
    delayMicroseconds(20);
    gpio_matrix_in((gpio_num_t)-1, U1RXD_IN_IDX, false);
    ESP_ERROR_CHECK(gpio_set_pull_mode((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, GPIO_FLOATING));
    ESP_ERROR_CHECK(gpio_set_pull_mode((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, GPIO_FLOATING));
    ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, 0));
    ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, GPIO_MODE_OUTPUT));
    gpio_matrix_out((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, U1TXD_OUT_IDX, true, false);

    //vTaskDelay(1 / portTICK_PERIOD_MS);
    yield();
}
