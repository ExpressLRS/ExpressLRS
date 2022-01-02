#include "crc.h"

GENERIC_CRC8::GENERIC_CRC8(uint8_t poly)
{
    uint8_t crc;

    for (uint16_t i = 0; i < crclen; i++)
    {
        crc = i;
        for (uint8_t j = 0; j < 8; j++)
        {
            crc = (crc << 1) ^ ((crc & 0x80) ? poly : 0);
        }
        crc8tab[i] = crc & 0xFF;
    }
}

uint8_t ICACHE_RAM_ATTR GENERIC_CRC8::calc(const uint8_t data)
{
    return crc8tab[data];
}

uint8_t ICACHE_RAM_ATTR GENERIC_CRC8::calc(const uint8_t *data, uint8_t len, uint16_t crc)
{
    while (len--)
    {
        crc = crc8tab[crc ^ *data++];
    }
    return crc;
}

GENERIC_CRC14::GENERIC_CRC14(uint16_t poly)
{
    uint16_t crc;
    for (uint16_t i = 0; i < crclen; i++)
    {
        crc = i << (14 - 8);
        for (uint8_t j = 0; j < 8; j++)
        {
            crc = (crc << 1) ^ ((crc & 0x2000) ? poly : 0);
        }
        crc14tab[i] = crc;
    }
}

uint16_t ICACHE_RAM_ATTR GENERIC_CRC14::calc(uint8_t *data, uint8_t len, uint16_t crc)
{
    while (len--)
    {
        crc = (crc << 8) ^ crc14tab[((crc >> 6) ^ (uint16_t) *data++) & 0x00FF];
    }
    return crc & 0x3FFF;
}

uint16_t ICACHE_RAM_ATTR GENERIC_CRC14::calc(volatile uint8_t *data, uint8_t len, uint16_t crc)
{
    while (len--)
    {
        crc = (crc << 8) ^ crc14tab[((crc >> 6) ^ (uint16_t) *data++) & 0x00FF];
    }
    return crc & 0x3FFF;
}
