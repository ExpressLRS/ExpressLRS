// Code to setup clocks and gpio on stm32f1
//
// Copyright (C) 2019  Kevin O'Connor <kevin@koconnor.net>
// https://github.com/KevinOConnor/klipper
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "gpio.h"
#include "gpio.h" // gpio_out_setup
#include "internal.h"
#include "helpers.h"
#include "irq.h"
#include <string.h> // ffs

GPIO_TypeDef *const digital_regs[] = {
    ['A' - 'A'] = GPIOA,
    GPIOB,
    GPIOC,
#ifdef GPIOD
    ['D' - 'A'] = GPIOD,
#endif
#ifdef GPIOE
    ['E' - 'A'] = GPIOE,
#endif
#ifdef GPIOF
    ['F' - 'A'] = GPIOF,
#endif
#ifdef GPIOG
    ['G' - 'A'] = GPIOG,
#endif
#ifdef GPIOH
    ['H' - 'A'] = GPIOH,
#endif
#ifdef GPIOI
    ['I' - 'A'] = GPIOI,
#endif
};

// Convert a register and bit location back to an integer pin identifier
static int
regs_to_pin(GPIO_TypeDef *regs, uint32_t bit)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(digital_regs); i++)
        if (digital_regs[i] == regs)
            return GPIO('A' + i, ffs(bit) - 1);
    return 0;
}

struct gpio_out
gpio_out_setup(uint32_t pin, uint32_t val)
{
    if (GPIO2PORT(pin) >= ARRAY_SIZE(digital_regs))
        goto fail;
    GPIO_TypeDef *regs = digital_regs[GPIO2PORT(pin)];
    if (!regs)
        goto fail;
    gpio_clock_enable(regs);
    struct gpio_out g = {.regs = regs, .bit = GPIO2BIT(pin)};
    gpio_out_reset(g, val);
    return g;
fail:
    while (1)
    {
    }
}

void gpio_out_reset(struct gpio_out g, uint32_t val)
{
    GPIO_TypeDef *regs = g.regs;
    int pin = regs_to_pin(regs, g.bit);
    irqstatus_t flag = irq_save();
    if (val)
        regs->BSRR = g.bit;
    else
        regs->BSRR = g.bit << 16;
    gpio_peripheral(pin, GPIO_OUTPUT, 0);
    irq_restore(flag);
}

void gpio_out_toggle_noirq(struct gpio_out g)
{
    GPIO_TypeDef *regs = g.regs;
    regs->ODR ^= g.bit;
}

void gpio_out_toggle(struct gpio_out g)
{
    irqstatus_t flag = irq_save();
    gpio_out_toggle_noirq(g);
    irq_restore(flag);
}

void gpio_out_write(struct gpio_out g, uint32_t val)
{
    GPIO_TypeDef *regs = g.regs;
    if (val)
        regs->BSRR = g.bit;
    else
        regs->BSRR = g.bit << 16;
}

struct gpio_in
gpio_in_setup(uint32_t pin, int32_t pull_up)
{
    if (GPIO2PORT(pin) >= ARRAY_SIZE(digital_regs))
        goto fail;
    GPIO_TypeDef *regs = digital_regs[GPIO2PORT(pin)];
    if (!regs)
        goto fail;
    struct gpio_in g = {.regs = regs, .bit = GPIO2BIT(pin)};
    gpio_in_reset(g, pull_up);
    return g;
fail:
    while (1)
    {
    }
}

void gpio_in_reset(struct gpio_in g, int32_t pull_up)
{
    GPIO_TypeDef *regs = g.regs;
    int pin = regs_to_pin(regs, g.bit);
    irqstatus_t flag = irq_save();
    gpio_peripheral(pin, GPIO_INPUT, pull_up);
    irq_restore(flag);
}

uint8_t
gpio_in_read(struct gpio_in g)
{
    GPIO_TypeDef *regs = g.regs;
    return !!(regs->IDR & g.bit);
}

/***************** Arduino.h compatibility *****************/

void pinMode(uint32_t pin, uint8_t mode)
{
    switch (mode)
    {
    case GPIO_INPUT:
        gpio_in_setup(pin, 0);
        break;
    case GPIO_OUTPUT:
        gpio_out_setup(pin, 1);
        break;
    case GPIO_INPUT_PULLUP:
        gpio_in_setup(pin, 1);
        break;
    }
}

void digitalWrite(uint32_t pin, uint8_t val)
{
    GPIO_TypeDef *regs = digital_regs[GPIO2PORT(pin)];
    uint32_t bit = GPIO2BIT(pin);
    // toggle => regs->ODR ^= bit;
    if (val)
        regs->BSRR = bit;
    else
        regs->BSRR = bit << 16;
}

uint8_t digitalRead(uint32_t pin)
{
    GPIO_TypeDef *regs = digital_regs[GPIO2PORT(pin)];
    uint32_t bit = GPIO2BIT(pin);
    return !!(regs->ODR & bit);
}
