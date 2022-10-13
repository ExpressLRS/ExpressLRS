/*
 * SPDX-FileCopyrightText: 2016-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* SoC-level support for ESP8266/ESP32.
 *
 * Provide a unified register-level interface.
 *
 * This is the same information provided in the register headers
 * of ESP8266 Non-OS SDK and ESP-IDF soc component, however
 * only values that are needed for the flasher stub are included here.
 *
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define READ_REG(REG) (*((volatile uint32_t *)(REG)))
#define WRITE_REG(REG, VAL) *((volatile uint32_t *)(REG)) = (VAL)
#define REG_SET_MASK(reg, mask) WRITE_REG((reg), (READ_REG(reg)|(mask)))
#define REG_CLR_MASK(reg, mask) WRITE_REG((reg), (READ_REG(reg)&(~(mask))))

#define ESP32_OR_LATER   !(ESP8266)
#define ESP32S2_OR_LATER !(ESP8266 || ESP32)
#define ESP32S3_OR_LATER !(ESP8266 || ESP32 || ESP32S2)

/**********************************************************
 * Per-SOC capabilities
 */
#ifdef ESP32S2
#define WITH_USB_OTG 1
#endif // ESP32S2

#ifdef ESP32C3
#define WITH_USB_JTAG_SERIAL 1
#define IS_RISCV 1
#endif // ESP32C3

#ifdef ESP32S3
#define WITH_USB_JTAG_SERIAL 1
#define WITH_USB_OTG 1
#endif // ESP32S3
