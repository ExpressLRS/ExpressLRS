/*
 *******************************************************************************
 * Copyright (c) 2021, STMicroelectronics
 * All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */

#ifndef _VARIANT_ARDUINO_STM32_
#define _VARIANT_ARDUINO_STM32_
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*----------------------------------------------------------------------------
 *        STM32 pins number
 *----------------------------------------------------------------------------*/
#define PA0                     A0
#define PA1                     A1
#define PA2                     A2
#define PA3                     A3
#define PA4                     A4
#define PA5                     A5
#define PA6                     A6
#define PA8                     7
#define PA9                     8
#define PA10                    9
#define PA11                    10
#define PA12                    11
#define PA13                    12
#define PA14                    13
#define PA15                    14
#define PB0                     A7
#define PB1                     A8
#define PB2                     17
#define PB3                     18
#define PB4                     19
#define PB5                     20
#define PB6                     21
#define PB7                     22
#define PB8                     23
#define PB9                     24
#define PB14                    25
#define PB15                    26
#define PC13                    27
#define PC14                    28
#define PC15                    29
#define PD8                     30
#define PE8                     31
#define PE9                     32
#define PF0                     33
#define PF1                     34
#define PF6                     35
#define PF7                     36

#define NUM_DIGITAL_PINS        37
#define NUM_ANALOG_INPUTS       9

// Timer Definitions
// Use TIM6/TIM7 when possible as servo and tone don't need GPIO output pin
#ifndef TIMER_TONE
#define TIMER_TONE              TIM6
#endif
#ifndef TIMER_SERVO
#define TIMER_SERVO             TIM7
#endif

// UART Definitions
#ifndef SERIAL_UART_INSTANCE
#define SERIAL_UART_INSTANCE    1
#endif

// Default pin used for generic 'Serial' instance
// Mandatory for Firmata
#ifndef PIN_SERIAL_RX
#define PIN_SERIAL_RX           PA10
#endif
#ifndef PIN_SERIAL_TX
#define PIN_SERIAL_TX           PA9
#endif

// Extra HAL modules
//#define HAL_DAC_MODULE_ENABLED

#define ALT1 (1 << (STM_PIN_AFNUM_SHIFT+0))
#define ALT2 (1 << (STM_PIN_AFNUM_SHIFT+1))
#define ALT3 (1 << (STM_PIN_AFNUM_SHIFT+2))
#define ALT4 (1 << (STM_PIN_AFNUM_SHIFT+3))

// Adjust IRQ priority
#define TIM_IRQ_PRIO            4
#define EXTI_IRQ_PRIO           4

#ifdef __cplusplus
} // extern "C"
#endif

/*----------------------------------------------------------------------------
 *        Arduino objects - C++ only
 *----------------------------------------------------------------------------*/

#ifdef __cplusplus
  // These serial port names are intended to allow libraries and architecture-neutral
  // sketches to automatically default to the correct port name for a particular type
  // of use.  For example, a GPS module would normally connect to SERIAL_PORT_HARDWARE_OPEN,
  // the first hardware serial port whose RX/TX pins are not dedicated to another use.
  //
  // SERIAL_PORT_MONITOR        Port which normally prints to the Arduino Serial Monitor
  //
  // SERIAL_PORT_USBVIRTUAL     Port which is USB virtual serial
  //
  // SERIAL_PORT_LINUXBRIDGE    Port which connects to a Linux system via Bridge library
  //
  // SERIAL_PORT_HARDWARE       Hardware serial port, physical RX & TX pins.
  //
  // SERIAL_PORT_HARDWARE_OPEN  Hardware serial ports which are open for use.  Their RX & TX
  //                            pins are NOT connected to anything by default.
  #define SERIAL_PORT_MONITOR   Serial
  #define SERIAL_PORT_HARDWARE  Serial
#endif

#endif /* _VARIANT_ARDUINO_STM32_ */