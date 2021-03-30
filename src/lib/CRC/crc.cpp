#include "crc.h"

GENERIC_CRC8::GENERIC_CRC8(uint8_t poly)
{
    poly = (poly << 1) + 1; // Correct Koopman formatting https://users.ece.cmu.edu/~koopman/crc/
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

GENERIC_CRC16::GENERIC_CRC16(uint16_t poly)
{
    poly = (poly << 1) + 1; // Correct Koopman formatting https://users.ece.cmu.edu/~koopman/crc/
    uint32_t i;
    uint8_t j;
    uint16_t crc;

    for (i = 0; i < crclen; i++)
    {
        crc = i << 8;
        for (j = 0; j < 8; j++)
        {
            crc = (crc << 1) ^ ((crc & 0x8000) ? poly : 0);
        }
        crc16tab[i] = crc;
    }
}

uint16_t ICACHE_RAM_ATTR GENERIC_CRC16::calc(uint8_t *data, uint8_t len, uint16_t crc)
{
    for (uint8_t i = 0; i < len; i++)
    {
        crc = (crc >> 8) ^ crc16tab[(crc ^ (uint16_t) *data++) & 0x00FF];
    }    
    return crc;
}

uint16_t ICACHE_RAM_ATTR GENERIC_CRC16::calc(volatile uint8_t *data, uint8_t len, uint16_t crc)
{
    for (uint8_t i = 0; i < len; i++)
    {
        crc = (crc >> 8) ^ crc16tab[(crc ^ (uint16_t) *data++) & 0x00FF];
    }
    return crc;
}

uint8_t ICACHE_RAM_ATTR getParity(uint8_t *data, uint8_t len)
{
    uint8_t parity = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        parity ^= __builtin_parity(*data++);
    }
    return parity;
}

uint8_t ICACHE_RAM_ATTR getParity(volatile uint8_t *data, uint8_t len)
{
    uint8_t parity = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        parity ^= __builtin_parity(*data++);
    }
    return parity;
}
