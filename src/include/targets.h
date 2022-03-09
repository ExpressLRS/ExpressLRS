#pragma once
#if !defined TARGET_NATIVE
#include <Arduino.h>
#endif

#define UNDEF_PIN (-1)

/// General Features ///
#define LED_MAX_BRIGHTNESS 50 //0..255 for max led brightness

/////////////////////////

#define WORD_ALIGNED_ATTR __attribute__((aligned(4)))

#ifdef PLATFORM_STM32
/* ICACHE_RAM_ATTR1 is always linked into RAM */
#define ICACHE_RAM_ATTR1  __section(".ram_code")
/* ICACHE_RAM_ATTR2 is linked into RAM only if enough space */
#if RAM_CODE_LIMITED
#define ICACHE_RAM_ATTR2
#else
#define ICACHE_RAM_ATTR2 __section(".ram_code")
#endif
#define ICACHE_RAM_ATTR //nothing//
#else
#undef ICACHE_RAM_ATTR //fix to allow both esp32 and esp8266 to use ICACHE_RAM_ATTR for mapping to IRAM
#define ICACHE_RAM_ATTR IRAM_ATTR
#endif

#if defined(TARGET_NATIVE)
#define IRAM_ATTR
#include "native.h"
#endif

#if defined(PLATFORM_STM32)
#ifdef GPIO_PIN_LED_WS2812
#ifndef GPIO_PIN_LED_WS2812_FAST
#error "WS2812 support requires _FAST pin!"
#endif
#else
#define GPIO_PIN_LED_WS2812         UNDEF_PIN
#define GPIO_PIN_LED_WS2812_FAST    UNDEF_PIN
#endif
#endif

#ifndef DMA_ATTR
#define DMA_ATTR
#endif

/* Set red led to default */
#ifndef GPIO_PIN_LED
#ifdef GPIO_PIN_LED_RED
#define GPIO_PIN_LED GPIO_PIN_LED_RED
#endif /* GPIO_PIN_LED_RED */
#endif /* GPIO_PIN_LED */

#ifndef GPIO_PIN_BUFFER_OE
#define GPIO_PIN_BUFFER_OE UNDEF_PIN
#endif
#ifndef GPIO_PIN_RST
#define GPIO_PIN_RST UNDEF_PIN
#endif
/* Temporary #undef to allow more testing without a BUSY pin. */
#ifdef GPIO_PIN_BUSY
#undef GPIO_PIN_BUSY
#define GPIO_PIN_BUSY UNDEF_PIN
#endif 
/* ---------------------------------------------------------- */
#ifndef GPIO_PIN_BUSY
#define GPIO_PIN_BUSY UNDEF_PIN
#endif
#ifndef GPIO_PIN_DIO0
#define GPIO_PIN_DIO0 UNDEF_PIN
#endif
#ifndef GPIO_PIN_DIO1
#define GPIO_PIN_DIO1 UNDEF_PIN
#endif
#ifndef GPIO_PIN_DIO2
#define GPIO_PIN_DIO2 UNDEF_PIN
#endif
#ifndef GPIO_PIN_PA_ENABLE
#define GPIO_PIN_PA_ENABLE UNDEF_PIN
#endif
#ifndef GPIO_BUTTON_INVERTED
#define GPIO_BUTTON_INVERTED 0
#endif
#ifndef GPIO_LED_RED_INVERTED
#define GPIO_LED_RED_INVERTED 0
#endif
#ifndef GPIO_LED_GREEN_INVERTED
#define GPIO_LED_GREEN_INVERTED 0
#endif
#ifndef GPIO_LED_BLUE_INVERTED
#define GPIO_LED_BLUE_INVERTED 0
#endif

#if !defined(BACKPACK_LOGGING_BAUD)
#define BACKPACK_LOGGING_BAUD 460800
#endif

#if defined(TARGET_TX)
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
#ifndef GPIO_PIN_DEBUG_RX
#define GPIO_PIN_DEBUG_RX       3
#endif
#ifndef GPIO_PIN_DEBUG_TX
#define GPIO_PIN_DEBUG_TX       1
#endif
#endif
#if defined(DEBUG_LOG) || defined(DEBUG_LOG_VERBOSE) || defined(USE_TX_BACKPACK)
#if GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_DEBUG_TX || GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_DEBUG_RX
#error "Cannot debug out the RC signal port!"
#endif
#if !defined(GPIO_PIN_DEBUG_RX) || !defined(GPIO_PIN_DEBUG_TX) || GPIO_PIN_DEBUG_RX == UNDEF_PIN || GPIO_PIN_DEBUG_TX == UNDEF_PIN
#error "When using DEBUG_LOG, DEBUG_LOG_VERBOSE or USE_TX_BACKPACK you must define both GPIO_PIN_DEBUG_RX and GPIO_PIN_DEBUG_TX"
#endif
#endif
#else // TARGET_RX
#if defined(PLATFORM_ESP8266)
#ifndef GPIO_PIN_DEBUG_RX
#define GPIO_PIN_DEBUG_RX       3
#endif
#ifndef GPIO_PIN_DEBUG_TX
#define GPIO_PIN_DEBUG_TX       1
#endif
#endif
#endif

#if defined(GPIO_PIN_SDA) && (GPIO_PIN_SDA != UNDEF_PIN) && defined(GPIO_PIN_SCL) && (GPIO_PIN_SCL != UNDEF_PIN)
#define USE_I2C
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

#elif defined(RADIO_SX127X)
#if !(defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_FCC_915) || \
        defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || \
        defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433) || \
        defined(UNIT_TEST))
#error "Regulatory_Domain is not defined for 900MHz device. Check user_defines.txt!"
#endif
#else
#error "Either RADIO_SX127X or RADIO_SX128X must be defined!"
#endif
