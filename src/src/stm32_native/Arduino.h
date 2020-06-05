#pragma once

#include "internal.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#define noInterrupts() __disable_irq()
#define interrupts() __enable_irq()

#define PA0  GPIO('A', 0)
#define PA1  GPIO('A', 1)
#define PA2  GPIO('A', 2)
#define PA3  GPIO('A', 3)
#define PA4  GPIO('A', 4)
#define PA5  GPIO('A', 5)
#define PA6  GPIO('A', 6)
#define PA7  GPIO('A', 7)
#define PA8  GPIO('A', 8)
#define PA9  GPIO('A', 9)
#define PA10 GPIO('A', 10)
#define PA11 GPIO('A', 11)
#define PA12 GPIO('A', 12)
#define PA13 GPIO('A', 13)
#define PA14 GPIO('A', 14)
#define PA15 GPIO('A', 15)
#define PB0  GPIO('B', 0)
#define PB1  GPIO('B', 1)
#define PB2  GPIO('B', 2)
#define PB3  GPIO('B', 3)
#define PB4  GPIO('B', 4)
#define PB5  GPIO('B', 5)
#define PB6  GPIO('B', 6)
#define PB7  GPIO('B', 7)
#define PB8  GPIO('B', 8)
#define PB9  GPIO('B', 9)
#define PB10 GPIO('B', 10)
#define PB11 GPIO('B', 11)
#define PB12 GPIO('B', 12)
#define PB13 GPIO('B', 13)
#define PB14 GPIO('B', 14)
#define PB15 GPIO('B', 15)
#define PC0  GPIO('C', 0)
#define PC1  GPIO('C', 1)
#define PC2  GPIO('C', 2)
#define PC3  GPIO('C', 3)
#define PC4  GPIO('C', 4)
#define PC5  GPIO('C', 5)
#define PC6  GPIO('C', 6)
#define PC7  GPIO('C', 7)
#define PC8  GPIO('C', 8)
#define PC9  GPIO('C', 9)
#define PC10 GPIO('C', 10)
#define PC11 GPIO('C', 11)
#define PC12 GPIO('C', 12)
#define PC13 GPIO('C', 13)
#define PC14 GPIO('C', 14)
#define PC15 GPIO('C', 15)
#define PD0  GPIO('D', 0)
#define PD1  GPIO('D', 1)
#define PD2  GPIO('D', 2)

#define LOW  0
#define HIGH 1

// ----------------------------------

uint32_t millis(void);
uint32_t micros(void);

void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);

// ----------------------------------

#define INPUT        GPIO_INPUT
#define OUTPUT       GPIO_OUTPUT
#define INPUT_PULLUP GPIO_INPUT_PULLUP

void pinMode(uint32_t pin, uint8_t mode);
void digitalWrite(uint32_t pin, uint8_t val);
uint8_t digitalRead(uint32_t pin);

enum
{
    FALLING = 1 << 4,
    RISING = 1 << 3,
    CHANGE = (FALLING + RISING)
};

typedef void (*isr_cb_t)(void);
#define digitalPinToInterrupt(pin) (pin)
void detachInterrupt(uint32_t);
void attachInterrupt(uint32_t pin, isr_cb_t cb, uint8_t type);

#define MICROSEC_COMPARE_FORMAT 1
