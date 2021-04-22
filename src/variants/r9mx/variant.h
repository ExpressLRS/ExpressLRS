/*
 *******************************************************************************
 * Copyright (c) 2019, STMicroelectronics
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
 *        Pins
 *----------------------------------------------------------------------------*/

#define  PA10 0 // SB33 ON / SB32 OFF
#define  PA9  1 // SB35 ON / SB34 OFF
#define  PA12 2
#define  PB3  3
#define  PB5  4
#define  PA15 5
#define  PB10 6
#define  PC7  7
#define  PB6  8
#define  PA8  9
#define  PA11 10
#define  PB15 11
#define  PB14 12
#define  PB13 13 // LED
#define  PB7  14
#define  PB8  15
// ST Morpho
// CN5 Left Side
#define  PC10 16
#define  PC12 17
#define  PB12 18
#define  PA13 19
#define  PA14 20
#define  PC13 21 // User Button
#define  PC14 22
#define  PC15  23
#define  PH0  24
#define  PH1  25
#define  PB4  26
#define  PB9  27
// CN5 Right Side
#define  PC11 28
#define  PD2  29
// CN6 Left Side
#define  PC9  30
// CN6 Right Side
#define  PC8  31
#define  PC6  32
#define  PC5  33
#define  PB0  A6
// #define  PA10 35  - Already defined as 0 (SB33 ON / SB32 OFF)
// #define  PA9  36  - Already defined as 1 (SB35 ON / SB34 OFF)
#define  PB11 37
#define  PB2  38
#define  PB1  A7
#define  PA7  A8
#define  PA6  A9
#define  PA5  A10
#define  PA4  A11
#define  PC4  44
#define  PA3  45 // STLink Rx
#define  PA2  46 // STLink Tx
#define  PA0  A0
#define  PA1  A1
#define  PC3  A2
#define  PC2  A3
#define  PC1  A4
#define  PC0  A5

// This must be a literal
#define NUM_DIGITAL_PINS        53

// This must be a literal
#define NUM_ANALOG_INPUTS       12

// On-board LED pin number
#define LED_BUILTIN             PB13
#define LED_GREEN               LED_BUILTIN

// On-board user button
#define USER_BTN                PC13

// Timer Definitions (optional)
// Use TIM6/TIM7 when possible as servo and tone don't need GPIO output pin
#define TIMER_TONE              TIM6
#define TIMER_SERVO             TIM7

// UART Definitions
// Define here Serial instance number to map on Serial generic name
#define SERIAL_UART_INSTANCE    0

// Default pin used for 'Serial' instance (ex: ST-Link)
// Mandatory for Firmata
#define PIN_SERIAL_RX           PA3
#define PIN_SERIAL_TX           PA2

// Enable DAC
#define HAL_DAC_MODULE_ENABLED

// Adjust IRQ priority
#define TIM_IRQ_PRIO            3
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
  #define SERIAL_PORT_MONITOR     Serial
  #define SERIAL_PORT_HARDWARE    Serial
#endif

#endif /* _VARIANT_ARDUINO_STM32_ */
