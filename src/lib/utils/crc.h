#ifndef __CRC_H_
#define __CRC_H_

#include "platform.h"
#include <stdint.h>

uint8_t ICACHE_RAM_ATTR CalcCRC(volatile uint8_t const *data, int length);
uint8_t ICACHE_RAM_ATTR CalcCRC(uint8_t const *data, int length);
uint8_t ICACHE_RAM_ATTR CalcCRCMsp(uint8_t *data, int length);

#if (CRSF_CMD_CRC)
uint8_t ICACHE_RAM_ATTR CalcCRCcmd(uint8_t const *data, int length);
#endif

#endif /* CRC_H_ */
