#include "elrs_eeprom.h"
#include <Arduino.h>
// #include "../../src/debug.h"

#ifdef PLATFORM_STM32
extEEPROM EEPROM(kbits_2, 1, 1);
#endif

void
ELRS_EEPROM::Begin()
{
#ifdef PLATFORM_STM32
    Wire.setSDA(GPIO_PIN_SDA); // set is needed or it wont work :/
    Wire.setSCL(GPIO_PIN_SCK);
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
        Serial.println("ERROR! EEPROM address is out of bounds");
        return 0;
    }
    return EEPROM.read(address);
}

void
ELRS_EEPROM::WriteByte(const uint16_t address, const uint8_t value)
{
    if (address >= RESERVED_EEPROM_SIZE) {
        // address is out of bounds
        Serial.println("ERROR! EEPROM address is out of bounds");
        return;
    }
    //Serial.printf("EEPROM: writing value = %u to address = %u\n", value, address);
    EEPROM.write(address, value);

#ifdef PLATFORM_ESP32
    if (EEPROM.commit()) {
      Serial.println("EEPROM successfully committed");
    }
    else {
      Serial.println("ERROR! EEPROM commit failed");
    }
#endif
}