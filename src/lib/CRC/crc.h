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
    explicit GENERIC_CRC8(uint8_t poly);
    uint8_t calc(uint8_t data);
    uint8_t calc(const uint8_t *data, uint16_t len, uint8_t crc = 0);
};

class Crc2Byte
{
private:
    uint16_t _crctab[crclen];
    uint8_t  _bits;
    uint16_t _bitmask;
    uint16_t _poly;

public:
    void init(uint8_t bits, uint16_t poly);
    uint16_t calc(uint8_t *data, uint8_t len, uint16_t crc);
};
