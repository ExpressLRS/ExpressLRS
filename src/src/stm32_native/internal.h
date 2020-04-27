// Code to setup clocks and gpio on stm32f1
//
// Copyright (C) 2019  Kevin O'Connor <kevin@koconnor.net>
// https://github.com/KevinOConnor/klipper
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __INTERNAL_H_
#define __INTERNAL_H_

#if defined(STM32F0xx)
#include "stm32f0xx.h"
#elif defined(STM32F1xx)
#include "stm32f1xx.h"
#elif defined(STM32F4xx)
#include "stm32f4xx.h"
#endif

extern GPIO_TypeDef *const digital_regs[];

#define GPIO(PORT, NUM) (((PORT) - 'A') * 16 + (NUM))
#define GPIO2PORT(PIN)  ((PIN) / 16)
#define GPIO2BIT(PIN)   (1U << GPIO2IDX(PIN))
#define GPIO2IDX(PIN)   ((PIN) % 16)

#define GPIO_INPUT        0
#define GPIO_OUTPUT       1
#define GPIO_INPUT_PULLUP 4
#define GPIO_OPEN_DRAIN   0x100
#define GPIO_FUNCTION(fn) (2 | ((fn) << 4))
#define GPIO_ANALOG       3

void enable_pclock(uint32_t periph_base);
int is_enabled_pclock(uint32_t periph_base);
uint32_t get_pclock_frequency(uint32_t periph_base);
void gpio_clock_enable(GPIO_TypeDef *regs);
void gpio_peripheral(uint32_t gpio, uint32_t mode, int pullup);

#endif
