/*
 * SPDX-FileCopyrightText: 2016-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Flasher command handlers, called from stub_flasher.c

   Commands related to writing flash are in stub_write_flash_xxx.
*/
#pragma once
#include <stdbool.h>
#include "soc_support.h"
#include "stub_flasher.h"

/* Get security info command only on ESP32S2 and later */
#if defined(HAS_SECURITY_INFO)
esp_command_error handle_get_security_info();
#endif
