#include "elrs_eeprom.h"
#include "targets.h"
#include "logging.h"

#if !defined(TARGET_NATIVE)
#include <EEPROM.h>

void
ELRS_EEPROM::Begin()
{
    EEPROM.begin(RESERVED_EEPROM_SIZE);
}

uint8_t
ELRS_EEPROM::ReadByte(const uint32_t address)
{
    if (address >= RESERVED_EEPROM_SIZE)
    {
        // address is out of bounds
        ERRLN("EEPROM address is out of bounds");
        return 0;
    }
    return EEPROM.read(address);
}

void
ELRS_EEPROM::WriteByte(const uint32_t address, const uint8_t value)
{
    if (address >= RESERVED_EEPROM_SIZE)
    {
        // address is out of bounds
        ERRLN("EEPROM address is out of bounds");
        return;
    }
    EEPROM.write(address, value);
}

void
ELRS_EEPROM::Commit()
{
    if (!EEPROM.commit())
    {
      ERRLN("EEPROM commit failed");
    }
}

#endif /* !TARGET_NATIVE */