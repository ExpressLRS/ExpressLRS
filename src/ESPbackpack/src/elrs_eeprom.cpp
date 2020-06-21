#include "elrs_eeprom.h"

#ifdef PLATFORM_STM32
extEEPROM EEPROM(kbits_2, 1, 1);
#endif

void
ELRS_EEPROM::Begin()
{
    EEPROM.begin(RESERVED_EEPROM_SIZE);
}

uint8_t
ELRS_EEPROM::ReadByte(const uint16_t address)
{
    return EEPROM.read(address);
}

void
ELRS_EEPROM::WriteByte(const uint16_t address, const uint8_t value)
{
    EEPROM.write(address, value);
}
