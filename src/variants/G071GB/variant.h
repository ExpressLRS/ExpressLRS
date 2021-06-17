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
#define PA0                     0   //A0
#define PA1                     1   //A1
#define PA2                     2   //A2
#define PA3                     3   //A3
#define PA4                     4   //A4
#define PA5                     5   //A5
#define PA6                     6   //A6
#define PA7                     7   //A7
#define PA8                     8
#define PA11                    9
#define PA12                    10
#define PA13                    11
#define PA14                    12
#define PA15                    13
#define PB0                     14   //A8
#define PB1                     15   //A9
#define PB3                     16
#define PB4                     17
#define PB5                     18
#define PB6                     19
#define PB7                     20
#define PB8                     21
#define PC6                     22
#define PC14                    23
#define PC15                    24
#define PF2                     25
#define PA9_R                   26
#define PA10_R                  27


#define NUM_DIGITAL_PINS        28
#define NUM_REMAP_PINS          2
#define NUM_ANALOG_INPUTS       10

// I2C definitions

#define PIN_WIRE_SDA            PA10_R
#define PIN_WIRE_SCL            PA9_R


// Timer Definitions
// Use TIM6/TIM7 when possible as servo and tone don't need GPIO output pin

#define TIMER_TONE              TIM6
#define TIMER_SERVO             TIM7


// UART Definitions
#ifndef SERIAL_UART_INSTANCE
#define SERIAL_UART_INSTANCE    1
#endif

// Default pin used for generic 'Serial' instance
// Mandatory for Firmata
#define PIN_SERIAL_RX           PB6
#define PIN_SERIAL_TX           PB7


// Extra HAL modules
//#define HAL_DAC_MODULE_ENABLED

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