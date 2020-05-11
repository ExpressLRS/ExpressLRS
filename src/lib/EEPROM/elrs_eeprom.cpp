#include "elrs_eeprom.h"

#ifdef PLATFORM_STM32
extEEPROM EEPROM(kbits_2, 1, 1);
#endif

void
ELRS_EEPROM::Begin()
{
#ifdef PLATFORM_STM32
    Wire.setSDA(GPIO_PIN_EEPROM_SDA); // set is needed or it wont work :/
    Wire.setSCL(GPIO_PIN_EEPROM_SCK);
    Wire.begin();
    EEPROM.begin();
#else
    EEPROM.begin(RESERVED_EEPROM_SIZE);
#endif
}

uint8_t
ELRS_EEPROM::ReadByte(const uint16_t address)
{
    if (address >= RESERVED_EEPROM_SIZE) {
        // address is out of bounds
        return 0;
    }
    return EEPROM.read(address);
}

void
ELRS_EEPROM::WriteByte(const uint16_t address, const uint8_t value)
{
    if (address >= RESERVED_EEPROM_SIZE) {
        // address is out of bounds
        return;
    }
    EEPROM.write(address, value);
}
