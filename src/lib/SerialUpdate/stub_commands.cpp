/*
 * SPDX-FileCopyrightText: 2019-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "targets.h"

#include <stdlib.h>
#include "stub_commands.h"
#include "stub_flasher.h"
#include "slip.h"

#if ESP32S2_OR_LATER
esp_command_error handle_get_security_info()
{
  uint8_t buf[SECURITY_INFO_BYTES];
  esp_command_error ret;

  ret = GetSecurityInfoProc(NULL, NULL, buf);
  if (ret == ESP_UPDATE_OK)
    SLIP_send_frame_data_buf(buf, sizeof(buf));
  return ret;
}
#endif // ESP32S2_OR_LATER
