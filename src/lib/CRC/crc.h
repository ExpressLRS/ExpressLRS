#pragma once
#include <stdint.h>
#include "targets.h"

#define crclen 256

class GENERIC_CRC8
{
private:
    uint8_t crc8tab[crclen];
    uint8_t crcpoly;

public:
    GENERIC_CRC8(uint8_t poly);
    uint8_t calc(const uint8_t data);
    uint8_t calc(const uint8_t *data, uint8_t len, uint8_t crc = 0);
};

class GENERIC_CRC14
{
private:
    uint16_t crc14tab[crclen];
    uint16_t crcpoly;

public:
    GENERIC_CRC14(uint16_t poly);
    uint16_t calc(uint8_t *data, uint8_t len, uint16_t crc);
    uint16_t calc(volatile uint8_t *data, uint8_t len, uint16_t crc);
};
