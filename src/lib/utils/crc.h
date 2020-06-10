#ifndef __CRC_H_
#define __CRC_H_

#include "platform.h"
#include <stdint.h>

uint8_t ICACHE_RAM_ATTR CalcCRC(uint8_t data, uint8_t crc);
uint8_t ICACHE_RAM_ATTR CalcCRC(volatile uint8_t const *data, uint16_t length, uint8_t crc = 0);
uint8_t ICACHE_RAM_ATTR CalcCRC(uint8_t const *data, uint16_t length, uint8_t crc = 0);
uint8_t ICACHE_RAM_ATTR CalcCRCxor(uint8_t *data, uint16_t length, uint8_t crc = 0);
uint8_t ICACHE_RAM_ATTR CalcCRCxor(uint8_t data, uint8_t crc = 0);
uint16_t ICACHE_RAM_ATTR CalcCRC16_XMODEM(uint8_t const *data, uint16_t length);
uint16_t ICACHE_RAM_ATTR CalcCRC16_CCITT(uint8_t const *data, uint16_t length, uint16_t crc = 0);

#define CalcCRC16(D, L, C) (CalcCRC16_CCITT((D), (L), (C)))
//#define CalcCRC16(D, L, C) (CalcCRC16_XMODEM((D), (L)) ^ (C))

uint8_t crc8_dvb_s2(uint8_t crc, uint8_t a);

#if (CRSF_CMD_CRC)
uint8_t ICACHE_RAM_ATTR CalcCRCcmd(uint8_t const *data, uint16_t length, uint8_t crc = 0);
#endif

#endif /* CRC_H_ */
