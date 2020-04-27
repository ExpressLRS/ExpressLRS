// Code to setup clocks and gpio on stm32f1
//
// Copyright (C) 2019  Kevin O'Connor <kevin@koconnor.net>
// https://github.com/KevinOConnor/klipper
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "gpio.h" // gpio_out_setup
#include "internal.h"
#include "helpers.h"
#include "irq.h"
#include <string.h> // ffs
#include <Arduino.h>

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
    uint32_t i;
    for (i = 0; i < ARRAY_SIZE(digital_regs); i++)
        if (digital_regs[i] == regs)
            return GPIO('A' + i, ffs(bit) - 1);
    return 0;
}

struct gpio_out
gpio_out_setup(uint32_t pin, uint32_t val)
{
    //if (GPIO2PORT(pin) >= ARRAY_SIZE(digital_regs))
    //    goto fail;
    GPIO_TypeDef *regs = digital_regs[GPIO2PORT(pin)];
    //if (!regs)
    //    goto fail;
    gpio_clock_enable(regs);
    struct gpio_out g = {.regs = regs, .bit = GPIO2BIT(pin)};
    gpio_out_reset(g, val);
    return g;
}

void gpio_out_reset(struct gpio_out g, uint32_t val)
{
    GPIO_TypeDef *regs = (GPIO_TypeDef *)g.regs;
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
    GPIO_TypeDef *regs = (GPIO_TypeDef *)g.regs;
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
    GPIO_TypeDef *regs = (GPIO_TypeDef *)g.regs;
    if (val)
        regs->BSRR = g.bit;
    else
        regs->BSRR = g.bit << 16;
}

struct gpio_in
gpio_in_setup(uint32_t pin, int32_t pull_up)
{
    //if (GPIO2PORT(pin) >= ARRAY_SIZE(digital_regs))
    //    goto fail;
    GPIO_TypeDef *regs = digital_regs[GPIO2PORT(pin)];
    //if (!regs)
    //    goto fail;
    struct gpio_in g = {.regs = regs, .bit = GPIO2BIT(pin)};
    gpio_in_reset(g, pull_up);
    return g;
    /*fail:
    while (1)
    {
    }*/
}

void gpio_in_reset(struct gpio_in g, int32_t pull_up)
{
    GPIO_TypeDef *regs = (GPIO_TypeDef *)g.regs;
    int pin = regs_to_pin(regs, g.bit);
    irqstatus_t flag = irq_save();
    gpio_peripheral(pin, GPIO_INPUT, pull_up);
    irq_restore(flag);
}

uint8_t
gpio_in_read(struct gpio_in g)
{
    GPIO_TypeDef *regs = (GPIO_TypeDef *)g.regs;
    return !!(regs->IDR & g.bit);
}

/***************** Arduino.h compatibility *****************/
#include <Arduino.h>

typedef struct
{
    IRQn_Type irqnb;
    isr_cb_t callback;
} gpio_irq_conf_str;

/* Private_Defines */
#define NB_EXTI (16)

/* Private Variables */
static gpio_irq_conf_str gpio_irq_conf[NB_EXTI] = {
#if defined(STM32F0xx) || defined(STM32G0xx) || defined(STM32L0xx)
    {.irqnb = EXTI0_1_IRQn, .callback = NULL},  //GPIO_PIN_0
    {.irqnb = EXTI0_1_IRQn, .callback = NULL},  //GPIO_PIN_1
    {.irqnb = EXTI2_3_IRQn, .callback = NULL},  //GPIO_PIN_2
    {.irqnb = EXTI2_3_IRQn, .callback = NULL},  //GPIO_PIN_3
    {.irqnb = EXTI4_15_IRQn, .callback = NULL}, //GPIO_PIN_4
    {.irqnb = EXTI4_15_IRQn, .callback = NULL}, //GPIO_PIN_5
    {.irqnb = EXTI4_15_IRQn, .callback = NULL}, //GPIO_PIN_6
    {.irqnb = EXTI4_15_IRQn, .callback = NULL}, //GPIO_PIN_7
    {.irqnb = EXTI4_15_IRQn, .callback = NULL}, //GPIO_PIN_8
    {.irqnb = EXTI4_15_IRQn, .callback = NULL}, //GPIO_PIN_9
    {.irqnb = EXTI4_15_IRQn, .callback = NULL}, //GPIO_PIN_10
    {.irqnb = EXTI4_15_IRQn, .callback = NULL}, //GPIO_PIN_11
    {.irqnb = EXTI4_15_IRQn, .callback = NULL}, //GPIO_PIN_12
    {.irqnb = EXTI4_15_IRQn, .callback = NULL}, //GPIO_PIN_13
    {.irqnb = EXTI4_15_IRQn, .callback = NULL}, //GPIO_PIN_14
    {.irqnb = EXTI4_15_IRQn, .callback = NULL}  //GPIO_PIN_15
#else
    {.irqnb = EXTI0_IRQn, .callback = NULL},     //GPIO_PIN_0
    {.irqnb = EXTI1_IRQn, .callback = NULL},     //GPIO_PIN_1
    {.irqnb = EXTI2_IRQn, .callback = NULL},     //GPIO_PIN_2
    {.irqnb = EXTI3_IRQn, .callback = NULL},     //GPIO_PIN_3
    {.irqnb = EXTI4_IRQn, .callback = NULL},     //GPIO_PIN_4
    {.irqnb = EXTI9_5_IRQn, .callback = NULL},   //GPIO_PIN_5
    {.irqnb = EXTI9_5_IRQn, .callback = NULL},   //GPIO_PIN_6
    {.irqnb = EXTI9_5_IRQn, .callback = NULL},   //GPIO_PIN_7
    {.irqnb = EXTI9_5_IRQn, .callback = NULL},   //GPIO_PIN_8
    {.irqnb = EXTI9_5_IRQn, .callback = NULL},   //GPIO_PIN_9
    {.irqnb = EXTI15_10_IRQn, .callback = NULL}, //GPIO_PIN_10
    {.irqnb = EXTI15_10_IRQn, .callback = NULL}, //GPIO_PIN_11
    {.irqnb = EXTI15_10_IRQn, .callback = NULL}, //GPIO_PIN_12
    {.irqnb = EXTI15_10_IRQn, .callback = NULL}, //GPIO_PIN_13
    {.irqnb = EXTI15_10_IRQn, .callback = NULL}, //GPIO_PIN_14
    {.irqnb = EXTI15_10_IRQn, .callback = NULL}  //GPIO_PIN_15
#endif
};

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

#define GPIO_MODE_IT  (1 << 0)
#define GPIO_MODE_EVT (1 << 1)

void attachInterrupt(uint32_t pin, isr_cb_t callback, uint8_t type)
{
    //GPIO_TypeDef *regs = digital_regs[GPIO2PORT(pin)];
    uint32_t index = GPIO2IDX(pin);
    uint32_t bit = GPIO2BIT(pin);
    uint32_t it_mode = type + GPIO_MODE_IT;

    /* Configure the External Interrupt or event for the current IO */

    /* Configure the interrupt mask */
    if ((it_mode & GPIO_MODE_IT) == GPIO_MODE_IT)
    {
        SET_BIT(EXTI->IMR, bit);
    }
    else
    {
        CLEAR_BIT(EXTI->IMR, bit);
    }

    /* Configure the event mask */
    if ((it_mode & GPIO_MODE_EVT) == GPIO_MODE_EVT)
    {
        SET_BIT(EXTI->EMR, bit);
    }
    else
    {
        CLEAR_BIT(EXTI->EMR, bit);
    }

    /* Enable or disable the rising trigger */
    if ((it_mode & RISING) == RISING)
    {
        SET_BIT(EXTI->RTSR, bit);
    }
    else
    {
        CLEAR_BIT(EXTI->RTSR, bit);
    }

    /* Enable or disable the falling trigger */
    if ((it_mode & FALLING) == FALLING)
    {
        SET_BIT(EXTI->FTSR, bit);
    }
    else
    {
        CLEAR_BIT(EXTI->FTSR, bit);
    }

    gpio_irq_conf[index].callback = callback;

    // Enable and set EXTI Interrupt

#ifndef EXTI_IRQ_PRIO
#if (__CORTEX_M == 0x00U)
#define EXTI_IRQ_PRIO 3
#else
#define EXTI_IRQ_PRIO 6
#endif /* __CORTEX_M */
#endif /* EXTI_IRQ_PRIO */
#ifndef EXTI_IRQ_SUBPRIO
#define EXTI_IRQ_SUBPRIO 0
#endif

    uint32_t prioritygroup = NVIC_GetPriorityGrouping();
    NVIC_SetPriority(gpio_irq_conf[index].irqnb, NVIC_EncodePriority(prioritygroup, EXTI_IRQ_PRIO, EXTI_IRQ_SUBPRIO));
    NVIC_EnableIRQ(gpio_irq_conf[index].irqnb);
}

void detachInterrupt(uint32_t pin)
{
    uint32_t index = GPIO2IDX(pin);
    irqstatus_t irq = irq_save();
    gpio_irq_conf[index].callback = NULL;
    irq_restore(irq);
}

/*********************/

void GPIO_EXTI_IRQHandler(uint16_t pin)
{
    /* EXTI line interrupt detected */
    if (EXTI->PR & pin)
    {
        EXTI->PR = pin;
        if (gpio_irq_conf[pin].callback != NULL)
        {
            gpio_irq_conf[pin].callback();
        }
    }
}

#ifdef __cplusplus
extern "C"
{
#endif
    /**
  * @brief This function handles external line 0 interrupt request.
  * @param  None
  * @retval None
  */
    void EXTI0_IRQHandler(void)
    {
        GPIO_EXTI_IRQHandler(0);
    }

    /**
  * @brief This function handles external line 1 interrupt request.
  * @param  None
  * @retval None
  */
    void EXTI1_IRQHandler(void)
    {
        GPIO_EXTI_IRQHandler(1);
    }

    /**
  * @brief This function handles external line 2 interrupt request.
  * @param  None
  * @retval None
  */
    void EXTI2_IRQHandler(void)
    {
        GPIO_EXTI_IRQHandler(2);
    }

    /**
  * @brief This function handles external line 3 interrupt request.
  * @param  None
  * @retval None
  */
    void EXTI3_IRQHandler(void)
    {
        GPIO_EXTI_IRQHandler(3);
    }

    /**
  * @brief This function handles external line 4 interrupt request.
  * @param  None
  * @retval None
  */
    void EXTI4_IRQHandler(void)
    {
        GPIO_EXTI_IRQHandler(4);
    }

    /**
  * @brief This function handles external line 5 to 9 interrupt request.
  * @param  None
  * @retval None
  */
    void EXTI9_5_IRQHandler(void)
    {
        uint8_t pin;
        for (pin = 5; pin <= 9; pin++)
        {
            GPIO_EXTI_IRQHandler(pin);
        }
    }

    /**
  * @brief This function handles external line 10 to 15 interrupt request.
  * @param  None
  * @retval None
  */
    void EXTI15_10_IRQHandler(void)
    {
        uint8_t pin;
        for (pin = 10; pin <= 15; pin++)
        {
            GPIO_EXTI_IRQHandler(pin);
        }
    }

#ifdef __cplusplus
}
#endif
