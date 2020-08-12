#pragma once

#include "../../src/targets.h"
#include <Wire.h>
#ifdef PLATFORM_STM32
    #include <extEEPROM.h>
#else
    #include <EEPROM.h>
#endif

#define RESERVED_EEPROM_SIZE    512

class ELRS_EEPROM
{
public:
    void Begin();
    uint8_t ReadByte(const uint16_t address);
    void WriteByte(const uint16_t address, const uint8_t value);
};
