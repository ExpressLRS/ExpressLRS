#ifndef CRC_H_
#define CRC_H_

#include <stdint.h>

uint8_t ICACHE_RAM_ATTR CalcCRC(volatile uint8_t const *data, int length);
uint8_t ICACHE_RAM_ATTR CalcCRC(uint8_t const *data, int length);

#if (CRSF_CMD_CRC)
uint8_t ICACHE_RAM_ATTR CalcCRCcmd(uint8_t const *data, int length);
#endif

#endif /* CRC_H_ */
