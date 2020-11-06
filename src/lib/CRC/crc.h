#pragma once
#include <Arduino.h>
#include "../../src/targets.h"

#define crclen 256

class GENERIC_CRC8
{
private:
    uint8_t crc8tab[crclen];
    uint8_t crcpoly;

public:
    GENERIC_CRC8(uint8_t poly);
    uint8_t calc(uint8_t *data, int len);
    uint8_t calc(volatile uint8_t *data, int len);
};
