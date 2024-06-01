/*
 * SPDX-FileCopyrightText: 2019-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "targets.h"

#if defined(PLATFORM_ESP32) && defined(TARGET_RX)

#include "soc_support.h"
#include "stub_commands.h"
#include "stub_flasher.h"
#include "slip.h"

#if defined(HAS_SECURITY_INFO)
#if defined(PLATFORM_ESP32_S3)
uint32_t (* GetSecurityInfoProc)(int* pMsg, int* pnErr, uint8_t *buf) = (uint32_t (*)(int* pMsg, int* pnErr, uint8_t *buf))0x40048174;  // pMsg and pnErr unused in ROM
#elif defined(PLATFORM_ESP32_C3)
uint32_t (* GetSecurityInfoProc)(int* pMsg, int* pnErr, uint8_t *buf) = (uint32_t (*)(int* pMsg, int* pnErr, uint8_t *buf))0x4004b9da;  // pMsg and pnErr unused in ROM
extern uint32_t _rom_eco_version; // rom constant to define ECO version
uint32_t (*GetSecurityInfoProcNewEco)(int* pMsg, int* pnErr, uint8_t *buf) = (uint32_t (*)(int* pMsg, int* pnErr, uint8_t *buf))0x4004d51e;  // GetSecurityInfo for C3 ECO7+
#endif

esp_command_error handle_get_security_info()
{
  uint8_t buf[SECURITY_INFO_BYTES];
  esp_command_error ret;

  #if defined(PLATFORM_ESP32_C3)
  if (_rom_eco_version >= 7)
    ret = (esp_command_error)GetSecurityInfoProcNewEco(NULL, NULL, buf);
  else
    ret = (esp_command_error)GetSecurityInfoProc(NULL, NULL, buf);
  #else
  ret = (esp_command_error)(*GetSecurityInfoProc)(NULL, NULL, buf);
  #endif // ESP32C3
  if (ret == ESP_UPDATE_OK)
    SLIP_send_frame_data_buf(buf, sizeof(buf));
  return ret;
}
#endif
#endif