#pragma once
#if !defined TARGET_NATIVE
#include <Arduino.h>
#endif

#define UNDEF_PIN (-1)

/// General Features ///
#define LED_MAX_BRIGHTNESS 50 //0..255 for max led brightness

/////////////////////////

#define WORD_ALIGNED_ATTR __attribute__((aligned(4)))
#define WORD_PADDED(size) (((size)+3) & ~3)

#undef ICACHE_RAM_ATTR //fix to allow both esp32 and esp8266 to use ICACHE_RAM_ATTR for mapping to IRAM
#define ICACHE_RAM_ATTR IRAM_ATTR

#if defined(TARGET_NATIVE)
#define IRAM_ATTR
#include "native.h"
#endif

#ifndef DMA_ATTR
#define DMA_ATTR
#endif

/*
 * Features
 * define features based on pins before defining pins as UNDEF_PIN
 */
#ifndef GPIO_PIN_BUFFER_OE
#define GPIO_PIN_BUFFER_OE UNDEF_PIN
#endif
#ifndef GPIO_PIN_SCK
#define GPIO_PIN_SCK UNDEF_PIN
#endif
#ifndef GPIO_PIN_NSS_2
#define GPIO_PIN_NSS_2 UNDEF_PIN
#endif
#ifndef GPIO_PIN_RST
#define GPIO_PIN_RST UNDEF_PIN
#endif
#ifndef GPIO_PIN_RST_2
#define GPIO_PIN_RST_2 UNDEF_PIN
#endif
#ifndef GPIO_PIN_BUSY
#define GPIO_PIN_BUSY UNDEF_PIN
#endif
#ifndef GPIO_PIN_BUSY_2
#define GPIO_PIN_BUSY_2 UNDEF_PIN
#endif
#ifndef GPIO_PIN_DIO0
#define GPIO_PIN_DIO0 UNDEF_PIN
#endif
#ifndef GPIO_PIN_DIO0_2
#define GPIO_PIN_DIO0_2 UNDEF_PIN
#endif
#ifndef GPIO_PIN_DIO1
#define GPIO_PIN_DIO1 UNDEF_PIN
#endif
#ifndef GPIO_PIN_DIO1_2
#define GPIO_PIN_DIO1_2 UNDEF_PIN
#endif
#ifndef GPIO_PIN_DIO2
#define GPIO_PIN_DIO2 UNDEF_PIN
#endif
#ifndef GPIO_PIN_PA_ENABLE
#define GPIO_PIN_PA_ENABLE UNDEF_PIN
#endif
#ifndef GPIO_PIN_RX_ENABLE
#define GPIO_PIN_RX_ENABLE UNDEF_PIN
#endif
#ifndef GPIO_PIN_RX_ENABLE_2
#define GPIO_PIN_RX_ENABLE_2 UNDEF_PIN
#endif
#ifndef GPIO_PIN_TX_ENABLE
#define GPIO_PIN_TX_ENABLE UNDEF_PIN
#endif
#ifndef GPIO_PIN_TX_ENABLE_2
#define GPIO_PIN_TX_ENABLE_2 UNDEF_PIN
#endif
#ifndef GPIO_PIN_ANT_CTRL
#define GPIO_PIN_ANT_CTRL UNDEF_PIN
#endif
#ifndef GPIO_PIN_ANT_CTRL_COMPL
#define GPIO_PIN_ANT_CTRL_COMPL UNDEF_PIN
#endif
#ifndef GPIO_PIN_RFamp_APC1
#define GPIO_PIN_RFamp_APC1 UNDEF_PIN
#endif
#ifndef GPIO_PIN_RFamp_APC2
#define GPIO_PIN_RFamp_APC2 UNDEF_PIN
#endif
#ifndef POWER_OUTPUT_VALUES2
#define POWER_OUTPUT_VALUES2 nullptr
#endif


#ifndef GPIO_PIN_FIVE_WAY_INPUT1
#define GPIO_PIN_FIVE_WAY_INPUT1 UNDEF_PIN
#endif
#ifndef GPIO_PIN_FIVE_WAY_INPUT2
#define GPIO_PIN_FIVE_WAY_INPUT2 UNDEF_PIN
#endif
#ifndef GPIO_PIN_FIVE_WAY_INPUT3
#define GPIO_PIN_FIVE_WAY_INPUT3 UNDEF_PIN
#endif


#ifndef GPIO_PIN_OLED_SCK
#define GPIO_PIN_OLED_SCK GPIO_PIN_SCL
#endif
#ifndef GPIO_PIN_OLED_SDA
#define GPIO_PIN_OLED_SDA GPIO_PIN_SDA
#endif

#if !defined(BACKPACK_LOGGING_BAUD)
#define BACKPACK_LOGGING_BAUD 460800
#endif
#ifndef GPIO_PIN_BACKPACK_BOOT
#define GPIO_PIN_BACKPACK_BOOT UNDEF_PIN
#endif

#ifndef GPIO_PIN_SPI_VTX_NSS
#define GPIO_PIN_SPI_VTX_NSS UNDEF_PIN
#endif


#if defined(TARGET_TX)
#if defined(PLATFORM_ESP32)
#ifndef GPIO_PIN_DEBUG_RX
#define GPIO_PIN_DEBUG_RX       3
#endif
#ifndef GPIO_PIN_DEBUG_TX
#define GPIO_PIN_DEBUG_TX       1
#endif
#elif defined(PLATFORM_ESP8266)
#ifndef GPIO_PIN_DEBUG_RX
#define GPIO_PIN_DEBUG_RX       UNDEF_PIN
#endif
#ifndef GPIO_PIN_DEBUG_TX
#define GPIO_PIN_DEBUG_TX       UNDEF_PIN
#endif
#endif
#else // TARGET_RX
#ifndef GPIO_PIN_RCSIGNAL_TX_SBUS
#define GPIO_PIN_RCSIGNAL_TX_SBUS UNDEF_PIN
#endif
#ifndef GPIO_PIN_RCSIGNAL_RX_SBUS
#define GPIO_PIN_RCSIGNAL_RX_SBUS UNDEF_PIN
#endif
#if defined(PLATFORM_ESP8266)
#ifndef GPIO_PIN_DEBUG_RX
#define GPIO_PIN_DEBUG_RX       3
#endif
#ifndef GPIO_PIN_DEBUG_TX
#define GPIO_PIN_DEBUG_TX       1
#endif
#endif
#endif

// Using these DEBUG_* imply that no SerialIO will be used so the output is readable
#if !defined(DEBUG_CRSF_NO_OUTPUT) && defined(TARGET_RX) && (defined(DEBUG_RCVR_LINKSTATS) || defined(DEBUG_RX_SCOREBOARD) || defined(DEBUG_RCVR_SIGNAL_STATS))
#define DEBUG_CRSF_NO_OUTPUT
#endif

#if defined(DEBUG_CRSF_NO_OUTPUT)
#define OPT_CRSF_RCVR_NO_SERIAL true
#elif defined(TARGET_RX)
extern bool pwmSerialDefined;

#define OPT_CRSF_RCVR_NO_SERIAL (GPIO_PIN_RCSIGNAL_RX == UNDEF_PIN && GPIO_PIN_RCSIGNAL_TX == UNDEF_PIN && !pwmSerialDefined)
#else
#define OPT_CRSF_RCVR_NO_SERIAL false
#endif

#if defined(RADIO_SX128X)
#define Regulatory_Domain_ISM_2400 1
// ISM 2400 band is in use => undefine other requlatory domain defines
#undef Regulatory_Domain_AU_915
#undef Regulatory_Domain_EU_868
#undef Regulatory_Domain_IN_866
#undef Regulatory_Domain_FCC_915
#undef Regulatory_Domain_AU_433
#undef Regulatory_Domain_EU_433
#undef Regulatory_Domain_US_433
#undef Regulatory_Domain_US_433_WIDE

#elif defined(RADIO_SX127X) || defined(RADIO_LR1121)
#if !(defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_FCC_915) || \
        defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || \
        defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433) || \
        defined(Regulatory_Domain_US_433) || defined(Regulatory_Domain_US_433_WIDE) || \
        defined(UNIT_TEST))
#error "Regulatory_Domain is not defined for 900MHz device. Check user_defines.txt!"
#endif
#else
#error "Either RADIO_SX127X, RADIO_LR1121 or RADIO_SX128X must be defined!"
#endif

#if defined(PLATFORM_ESP32)
#include <soc/uart_pins.h>
#endif
#if !defined(U0RXD_GPIO_NUM)
#define U0RXD_GPIO_NUM (3)
#endif
#if !defined(U0TXD_GPIO_NUM)
#define U0TXD_GPIO_NUM (1)
#endif
#include "hardware.h"
