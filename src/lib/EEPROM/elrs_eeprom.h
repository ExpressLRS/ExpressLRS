#pragma once

#include <Wire.h>
#include "../../src/targets.h"
#ifdef PLATFORM_STM32
    #include <extEEPROM.h>
#else
    #include <EEPROM.h>
#endif

#define RESERVED_EEPROM_SIZE    32

#define EEPROM_ADDR_UID2        1
#define EEPROM_ADDR_UID3        2
#define EEPROM_ADDR_UID4        3
#define EEPROM_ADDR_UID5        4
#define EEPROM_ADDR_POWER_LEVEL 5
#define EEPROM_ADDR_PKT_RATE    6
#define EEPROM_ADDR_TLM_RATE    7

class ELRS_EEPROM
{
public:
    void Begin();
    uint8_t ReadByte(const uint16_t address);
    void WriteByte(const uint16_t address, const uint8_t value);
};
