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

/*----------------------------------------------------------------------------
 *        Pins
 *----------------------------------------------------------------------------*/

// #define PA10 0    //
#define PA10 0    //
// #define PA9  1    //
#define PA9  1    //
// #define PA12 2    //
#define PA12 2    // 
#define PB0  3    // RESET --------------------------------- #define GPIO_PIN_RST
#define PB14  4    // TX_ENABLE ------------------------------#define GPIO_PIN_TX_ENABLE
#define PB10  5    // USART1, half duplex ------------------- #define GPIO_PIN_RCSIGNAL_RX
#define PA8  6    // RX_ENABLE ----------------------------- #define GPIO_PIN_RX_ENABLE
#define PF0  7    // XO, don't use!
#define PF1  8    // XO, don't use!
#define PA13  9    // ??????
#define PA11 10   // ??????
#define PA7  11   // MOSI pin ------------------------------ #define GPIO_PIN_MISO
#define PA6  12   // MISO pin ------------------------------ #define GPIO_PIN_MISO
#define PA5  13   // SCK ----------------------------------- #define GPIO_PIN_SCK
#define PA0  14   // ???????
#define PB2  15   // DIO1 ---------------------------------- #define GPIO_PIN_DIO1
#define PB15  16   // BUSY pin ------------------------------ #define GPIO_PIN_BUSY
#define PA4  17   // ???????
#define PB3  18   // if SB18 ON (default) connected to PB7 - 
#define PB4  19   // if SB16 ON (default) connected to PB6 - 
#define PB6  20   // LED ----------------------------------- #define GPIO_PIN_LED_WS2812
#define PA2  21   // STLink Tx ----------------------------- #define GPIO_PIN_RCSIGNAL_TX
#define PA15 22   // NSS ----------------------------------- #define GPIO_PIN_NSS



// This must be a literal
#define NUM_DIGITAL_PINS        23
// This must be a literal with a value less than or equal to to MAX_ANALOG_INPUTS
#define NUM_ANALOG_INPUTS       9

// I2C Definitions
#define PIN_WIRE_SDA            NC //4
#define PIN_WIRE_SCL            NC //5

// Timer Definitions
// Use TIM6/TIM7 when possible as servo and tone don't need GPIO output pin
#define TIMER_TONE              TIM6
#define TIMER_SERVO             TIM7

// UART Definitions
#define SERIAL_UART_INSTANCE    2 //Connected to ST-Link

// Serial Pin Firmata
#define PIN_SERIAL_RX           PA8
#define PIN_SERIAL_TX           PB14

/* Extra HAL modules */
//#define HAL_DAC_MODULE_ENABLED
#define HAL_DAC_MODULE_DISABLED
#define HAL_ADC_MODULE_DISABLED
//#define HAL_I2C_MODULE_DISABLED

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
