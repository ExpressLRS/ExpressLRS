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

#ifndef DMA_ATTR
#define DMA_ATTR
#endif

/*
 * Features
 * define features based on pins before defining pins as UNDEF_PIN
 */
#if defined(GPIO_PIN_FAN_EN)
#define HAS_FAN
#endif
#if defined(USE_OLED_I2C) || defined(USE_OLED_SPI) || defined(USE_OLED_SPI_SMALL) || defined(HAS_TFT_SCREEN)
#define HAS_SCREEN
#endif
#if defined(GPIO_PIN_SPI_VTX_NSS)
#if !defined(HAS_VTX_SPI)
#define HAS_VTX_SPI
#define OPT_HAS_VTX_SPI true
#endif
#else
#define OPT_HAS_VTX_SPI false
#endif

#ifndef USE_TX_BACKPACK
#define OPT_USE_TX_BACKPACK false
#elif !defined(OPT_USE_TX_BACKPACK)
#define OPT_USE_TX_BACKPACK true
#endif

#ifndef HAS_THERMAL
#define OPT_HAS_THERMAL false
#elif !defined(OPT_HAS_THERMAL)
#define OPT_HAS_THERMAL true
#endif

#ifndef HAS_GSENSOR
#define OPT_HAS_GSENSOR false
#elif !defined(OPT_HAS_GSENSOR)
#define OPT_HAS_GSENSOR true
#endif

#if defined(GPIO_PIN_SDA) && defined(GPIO_PIN_SCL)
#define USE_I2C
#else
#define GPIO_PIN_SDA UNDEF_PIN
#define GPIO_PIN_SCL UNDEF_PIN
#endif

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
#ifndef POWER_OUTPUT_FIXED
#define POWER_OUTPUT_FIXED -99
#endif
#ifndef GPIO_PIN_FAN_EN
#define GPIO_PIN_FAN_EN UNDEF_PIN
#endif
#ifndef GPIO_PIN_FAN_PWM
#define GPIO_PIN_FAN_PWM UNDEF_PIN
#endif
#ifndef GPIO_PIN_FAN_TACHO
#define GPIO_PIN_FAN_TACHO UNDEF_PIN
#endif
#ifndef GPIO_PIN_OLED_MOSI
#define GPIO_PIN_OLED_MOSI UNDEF_PIN
#endif
#ifndef GPIO_PIN_OLED_CS
#define GPIO_PIN_OLED_CS UNDEF_PIN
#endif
#ifndef GPIO_PIN_OLED_DC
#define GPIO_PIN_OLED_DC UNDEF_PIN
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
#if !defined(TARGET_UNIFIED_TX)
#if defined(DEBUG_LOG) || defined(DEBUG_LOG_VERBOSE) || defined(USE_TX_BACKPACK)
#if GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_DEBUG_TX || GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_DEBUG_RX
#error "Cannot debug out the RC signal port!"
#endif
#if !defined(GPIO_PIN_DEBUG_RX) || !defined(GPIO_PIN_DEBUG_TX) || GPIO_PIN_DEBUG_RX == UNDEF_PIN || GPIO_PIN_DEBUG_TX == UNDEF_PIN
#error "When using DEBUG_LOG, DEBUG_LOG_VERBOSE or USE_TX_BACKPACK you must define both GPIO_PIN_DEBUG_RX and GPIO_PIN_DEBUG_TX"
#endif
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

#if defined(TARGET_UNIFIED_RX)
extern bool pwmSerialDefined;

#define OPT_CRSF_RCVR_NO_SERIAL (GPIO_PIN_RCSIGNAL_RX == UNDEF_PIN && GPIO_PIN_RCSIGNAL_TX == UNDEF_PIN && !pwmSerialDefined)
#else
#if defined(CRSF_RCVR_NO_SERIAL)
#define OPT_CRSF_RCVR_NO_SERIAL true
#else
#define OPT_CRSF_RCVR_NO_SERIAL false
#endif
#endif

#if defined(USE_ANALOG_VBAT) && !defined(GPIO_ANALOG_VBAT)
#define GPIO_ANALOG_VBAT        A0
#if !defined(ANALOG_VBAT_SCALE)
#define ANALOG_VBAT_SCALE       1
#endif
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

#if defined(TARGET_UNIFIED_TX) || defined(TARGET_UNIFIED_RX)
#include "hardware.h"
#endif
