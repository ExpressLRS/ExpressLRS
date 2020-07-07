#ifndef ELRS_EEPROM_H
#define ELRS_EEPROM_H

#define RESERVED_EEPROM_SIZE 524288

#include <Wire.h>
#include <EEPROM.h>

class ELRS_EEPROM
{
public:
    void Begin();
    uint8_t ReadByte(const uint16_t address);
    void WriteByte(const uint16_t address, const uint8_t value);
};

#endif
