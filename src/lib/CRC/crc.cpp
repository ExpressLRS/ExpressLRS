#include "crc.h"

GENERIC_CRC8::GENERIC_CRC8(uint8_t poly)
{
    uint32_t i;
    uint8_t j;
    uint8_t crc;

    for (i = 0; i < crclen; i++)
    {
        crc = i;
        for (j = 0; j < 8; j++)
        {
            crc = (crc << 1) ^ ((crc & 0x80) ? poly : 0);
        }
        crc8tab[i] = crc & 0xFF;
    }
}

uint8_t ICACHE_RAM_ATTR GENERIC_CRC8::calc(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        crc = crc8tab[crc ^ *data++];
    }
    return crc;
}

uint8_t ICACHE_RAM_ATTR GENERIC_CRC8::calc(volatile uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        crc = crc8tab[crc ^ *data++];
    }
    return crc;
}
