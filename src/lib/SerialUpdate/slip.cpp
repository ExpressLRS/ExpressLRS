/*
 * SPDX-FileCopyrightText: 2016 Cesanta Software Limited
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * SPDX-FileContributor: 2016-2022 Espressif Systems (Shanghai) CO LTD
 */

#include "targets.h"
#include <stdint.h>
#include "slip.h"

#include <HardwareSerial.h>

void SLIP_send_frame_delimiter(void) {
  Serial.write('\xc0');
}

void SLIP_send_frame_data(char ch) {
  if(ch == '\xc0') {
	Serial.write('\xdb');
	Serial.write('\xdc');
  } else if (ch == '\xdb') {
	Serial.write('\xdb');
	Serial.write('\xdd');
  } else {
	Serial.write(ch);
  }
}

void SLIP_send_frame_status(char status) {
	SLIP_send_frame_data(status != 0 ? 1 : 0);
	SLIP_send_frame_data(status);
}

void SLIP_send_frame_data_buf(const void *buf, uint32_t size) {
  const uint8_t *buf_c = (const uint8_t *)buf;
  for(int i = 0; i < size; i++) {
	SLIP_send_frame_data(buf_c[i]);
  }
}

void SLIP_send(const void *pkt, uint32_t size) {
  SLIP_send_frame_delimiter();
  SLIP_send_frame_data_buf(pkt, size);
  SLIP_send_frame_delimiter();
}

int16_t SLIP_recv_byte(char byte, slip_state_t *state)
{
  if (byte == '\xc0') {
	if (*state == SLIP_NO_FRAME) {
	  *state = SLIP_FRAME;
	  return SLIP_NO_BYTE;
	} else {
	  *state = SLIP_NO_FRAME;
	  return SLIP_FINISHED_FRAME;
	}
  }

  switch(*state) {
  case SLIP_NO_FRAME:
	return SLIP_NO_BYTE;
  case SLIP_FRAME:
	if (byte == '\xdb') {
	  *state = SLIP_FRAME_ESCAPING;
	  return SLIP_NO_BYTE;
	}
	return byte;
  case SLIP_FRAME_ESCAPING:
	if (byte == '\xdc') {
	  *state = SLIP_FRAME;
	  return '\xc0';
	}
	if (byte == '\xdd') {
	  *state = SLIP_FRAME;
	  return '\xdb';
	}
	return SLIP_NO_BYTE; /* actually a framing error */
  }
  return SLIP_NO_BYTE; /* actually a framing error */
}
