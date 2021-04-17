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

uint8_t ICACHE_RAM_ATTR GENERIC_CRC8::calc(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    while (len--)
    {
        crc = crc8tab[crc ^ *data++];
    }
    return crc;
}

uint8_t ICACHE_RAM_ATTR GENERIC_CRC8::calc(volatile uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    while (len--)
    {
        crc = crc8tab[crc ^ *data++];
    }
    return crc;
}

GENERIC_CRC13::GENERIC_CRC13(uint16_t poly)
{
    uint16_t crc;
    for (uint16_t i = 0; i < crclen; i++)
    {
        crc = i << (13 - 8);
        for (uint8_t j = 0; j < 8; j++)
        {
            crc = (crc << 1) ^ ((crc & 0x1000) ? poly : 0);
        }
        crc13tab[i] = crc;
    }
}

uint16_t ICACHE_RAM_ATTR GENERIC_CRC13::calc(uint8_t *data, uint8_t len, uint16_t crc)
{
    while (len--)
    {
        crc = (crc >> 8) ^ crc13tab[(crc ^ (uint16_t) *data++) & 0x00FF];
    }    
    return crc & 0x1FFF;
}

uint16_t ICACHE_RAM_ATTR GENERIC_CRC13::calc(volatile uint8_t *data, uint8_t len, uint16_t crc)
{
    while (len--)
    {
        crc = (crc >> 8) ^ crc13tab[(crc ^ (uint16_t) *data++) & 0x00FF];
    }
    return crc & 0x1FFF;
}

uint8_t ICACHE_RAM_ATTR getParity(uint8_t *data, uint8_t len)
{
    uint8_t parity = 0;
    while (len--)
    {
        parity ^= __builtin_parity(*data++);
    }
    return parity;
}

uint8_t ICACHE_RAM_ATTR getParity(volatile uint8_t *data, uint8_t len)
{
    uint8_t parity = 0;
    while (len--)
    {
        parity ^= __builtin_parity(*data++);
    }
    return parity;
}
