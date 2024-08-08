/*
  Copyright (c) 2011 Arduino.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _VARIANT_ARDUINO_STM32_
#define _VARIANT_ARDUINO_STM32_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// void SystemClock_Config(void);
// #define FRSKY_R9MM
#define M0139
/*----------------------------------------------------------------------------
 *        Pins
 *----------------------------------------------------------------------------*/

#define PA3  0
#define PA2  1
#define PA10 2
#define PB3  3
#define PB5  4
#define PB4  5
#define PB10 6
#define PA8  7
#define PA9  8
#define PC7  9
#define PB6  10
#define PA7  11 // A6
#define PA6  12 // A7
#define PA5  13 // A8 - LED
#define PB9  14
#define PB8  15
// ST Morpho
// CN7 Left Side
#define PC10 16
#define PC12 17
// 18 is NC - BOOT0
#define PA13 19 // SWD
#define PA14 20 // SWD
#define PA15 21
#define PB7  22
#define PC13 23
#define PC14 24
#define PC15 25
#define PD0  26
#define PD1  27
#define PC2  28 // A9
#define PC3  29 // A10
// CN7 Right Side
#define PC11 30
#define PD2  31
// CN10 Left Side
#define PC9  32
// CN10 Right side
#define PC8  33
#define PC6  34
#define PC5  35 // A13
#define PA12 36
#define PA11 37
#define PB12 38
#define PB11 39
#define PB2  40
#define PB1  41 // A11
#define PB15 42
#define PB14 43
#define PB13 44
#define PC4  45 // A12
#define PA0  46 // A0
#define PA1  47 // A1
#define PA4  48 // A2
#define PB0  49 // A3
#define PC1  50 // A4
#define PC0  51 // A5

// This must be a literal
#define NUM_DIGITAL_PINS        60
// This must be a literal with a value less than or equal to to MAX_ANALOG_INPUTS
#define NUM_ANALOG_INPUTS       14
#define NUM_ANALOG_FIRST        46

// On-board LED pin number
#define LED_BUILTIN             PB3
#define LED_GREEN               LED_BUILTIN

// On-board user button
#define USER_BTN                PA1

// Timer Definitions
// Use TIM6/TIM7 when possible as servo and tone don't need GPIO output pin
#define TIMER_TONE              TIM3
#define TIMER_SERVO             TIM4

// UART Definitions
#define SERIAL_UART_INSTANCE    1 //Connected to ST-Link
// Default pin used for 'Serial' instance (ex: ST-Link)
// Mandatory for Firmata
// #define PIN_SERIAL_RX           0
// #define PIN_SERIAL_TX           1
#define PIN_SERIAL_RX           PA3
#define PIN_SERIAL_TX           PA2

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
#define SERIAL_PORT_MONITOR     Serial
#define SERIAL_PORT_HARDWARE    Serial
#endif

#endif /* _VARIANT_ARDUINO_STM32_ */
