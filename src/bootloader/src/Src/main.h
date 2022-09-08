/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

/* Includes ------------------------------------------------------------------*/
#ifdef STM32L0xx
#include "stm32l0xx.h"
#include "stm32l0xx_hal.h"
#include "stm32l0xx_ll_gpio.h"
#include "stm32l0xx_ll_usart.h"
#elif defined(STM32L1xx)
#include "stm32l1xx.h"
#include "stm32l1xx_hal.h"
#include "stm32l1xx_ll_gpio.h"
#include "stm32l1xx_ll_usart.h"
#elif defined(STM32F030x8)
#include "stm32f0xx.h"
#include "stm32f0xx_hal.h"
#include "stm32f0xx_ll_gpio.h"
#include "stm32f0xx_ll_usart.h"
#elif defined(STM32F1)
#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_usart.h"
#elif defined(STM32F3xx)
#include "stm32f3xx.h"
#include "stm32f3xx_hal.h"
#include "stm32f3xx_ll_gpio.h"
#include "stm32f3xx_ll_usart.h"
#elif defined(STM32L4xx)
#include "stm32l4xx.h"
#include "stm32l4xx_hal.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_usart.h"
#else
#error "Not supported CPU type!"
#endif

#define STRINGIFY(str) #str
#define BUILD_VERSION(ver) STRINGIFY(ver)
#define BUILD_MCU_TYPE(type) STRINGIFY(BL_TYPE: type)

#ifndef UART_TX_PIN
#error "Invalid UART config! TX pin is mandatory"
#endif

#ifndef UART_BAUD
#define UART_BAUD 420000
#endif
#ifndef UART_INV
#define UART_INV 0
#endif
#ifndef UART_BAUD_2ND
#define UART_BAUD_2ND UART_BAUD
#endif
#ifndef UART_INV_2ND
#define UART_INV_2ND 0
#endif


/* Private includes ----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

enum {
  IO_PORT_A = 'A',
  IO_PORT_B = 'B',
  IO_PORT_C = 'C',
  IO_PORT_D = 'D',
  IO_PORT_E = 'E',
  IO_PORT_F = 'F'
};
#define CONCAT(_P) IO_PORT_ ## _P
#define IO_CREATE_IMPL(port, pin) ((CONCAT(port) * 16) + pin)
#define IO_CREATE(...)  IO_CREATE_IMPL(__VA_ARGS__)
#define IO_GET_PORT(_p) (((_p) / 16) - 'A')
#define IO_GET_PIN(_p)  ((_p) % 16)
#define GPIO(R, P)      (((R) * 16) + (P))

#define GPIO_INPUT        0
#define GPIO_OUTPUT       1
#define GPIO_AF           2
#define GPIO_ANALOG       3
#define GPIO_OPEN_DRAIN   0x100
#define GPIO_FUNCTION(fn) (GPIO_AF | ((fn) << 4))

struct gpio_pin {
  GPIO_TypeDef *reg;
  uint32_t bit;
};
struct gpio_pin GPIO_Setup(uint32_t pin, uint32_t mode, int pullup);
static inline void GPIO_Write(struct gpio_pin gpio, uint32_t state) {
  if (state)
    gpio.reg->BSRR = gpio.bit;
  else
    gpio.reg->BSRR = (gpio.bit << 16);
}
static inline uint8_t GPIO_Read(struct gpio_pin gpio) {
  return !!(gpio.reg->IDR & gpio.bit);
}

void GPIO_SetupPin(GPIO_TypeDef *regs, uint32_t pos, uint32_t mode, int pullup);


enum led_states
{
  LED_OFF,
  LED_BOOTING,
  LED_FLASHING,
  LED_FLASHING_ALT,
  LED_STARTING,
};

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void led_state_set(uint32_t state);
int8_t boot_wait_timer_end(void);

void gpio_port_pin_get(uint32_t io, void ** port, uint32_t * pin);


/* Private defines -----------------------------------------------------------*/
#if !defined(XMODEM) && !STK500 && !FRSKY
#define XMODEM 1 // Default is XMODEM protocol
#endif


#define barrier() __asm__ __volatile__("": : :"memory")

static inline void write_u8(void *addr, uint8_t val) {
    barrier();
    *(volatile uint8_t *)addr = val;
}
static inline uint8_t read_u8(const void *addr) {
    uint8_t val = *(volatile const uint8_t *)addr;
    barrier();
    return val;
}

#if defined(STM32L0xx)
#define NVIC_EncodePriority(_x, _y, _z) (_y)
#define NVIC_GetPriorityGrouping() 0
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
