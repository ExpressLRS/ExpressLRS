#pragma once

#include "internal.h"
#include <stdint.h>
#include <stddef.h>

#define PA2 GPIO('A', 2)
#define PA3 GPIO('A', 3)
#define PA9 GPIO('A', 9)
#define PA10 GPIO('A', 10)
#define PA15 GPIO('A', 15)
#define PB3 GPIO('B', 3)
#define PB11 GPIO('B', 11)
#define PB12 GPIO('B', 12)
#define PB13 GPIO('B', 13)
#define PB14 GPIO('B', 14)
#define PB15 GPIO('B', 15)
#define PC1 GPIO('C', 1)
#define PC13 GPIO('C', 13)
#define PC14 GPIO('C', 14)

#define LOW 0
#define HIGH 1

void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
uint32_t millis(void);
uint32_t micros(void);

#define INPUT GPIO_INPUT
#define OUTPUT GPIO_OUTPUT
#define INPUT_PULLUP GPIO_INPUT_PULLUP

void pinMode(uint32_t pin, uint8_t mode);
void digitalWrite(uint32_t pin, uint8_t val);
uint8_t digitalRead(uint32_t pin);

#define RISING 1
typedef void (*isr_cb_t)(void);
uint32_t digitalPinToInterrupt(uint32_t);
void detachInterrupt(uint32_t);
void attachInterrupt(uint32_t pin, isr_cb_t cb, uint8_t type);

#define MICROSEC_COMPARE_FORMAT 1
