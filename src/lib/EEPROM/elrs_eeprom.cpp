#include "elrs_eeprom.h"
#include <Arduino.h>

#if defined(TARGET_R9M_TX) || defined(TARGET_R9M_LITE_TX)
    extEEPROM EEPROM(kbits_2, 1, 1, 0x51);
#endif

#ifdef TARGET_R9M_TX
    extEEPROM EEPROM(kbits_2, 1, 1, 0x50);
#endif

void
ELRS_EEPROM::Begin()
{
#ifdef PLATFORM_STM32
    EEPROM.begin();
#else
    EEPROM.begin(RESERVED_EEPROM_SIZE);
#endif
}

uint8_t
ELRS_EEPROM::ReadByte(const unsigned long address)
{
    if (address >= RESERVED_EEPROM_SIZE)
    {
        // address is out of bounds
        Serial.println("ERROR! EEPROM address is out of bounds");
        return 0;
    }
    return EEPROM.read(address);
}

void
ELRS_EEPROM::WriteByte(const unsigned long address, const uint8_t value)
{
    if (address >= RESERVED_EEPROM_SIZE)
    {
        // address is out of bounds
        Serial.println("ERROR! EEPROM address is out of bounds");
        return;
    }
    EEPROM.write(address, value);
}

void
ELRS_EEPROM::Commit()
{
#ifdef PLATFORM_ESP32
    if (!EEPROM.commit())
    {
      Serial.println("ERROR! EEPROM commit failed");
    }
#endif
}
