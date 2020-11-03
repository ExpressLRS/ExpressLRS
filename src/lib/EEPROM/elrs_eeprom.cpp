#include "elrs_eeprom.h"
#include <Arduino.h>

#if defined(TARGET_R9M_TX) || defined(TARGET_R9M_LITE_TX) || defined(TARGET_R9M_LITE_PRO_TX)
    extEEPROM EEPROM(kbits_2, 1, 1, 0x51);
#elif defined(TARGET_R9M_RX)
    extEEPROM EEPROM(kbits_2, 1, 1, 0x50);
#else
#define NO_EEPROM 1
#endif

void
ELRS_EEPROM::Begin()
{
#if !NO_EEPROM
#ifdef PLATFORM_STM32
    Wire.setSDA(GPIO_PIN_SDA); // set is needed or it wont work :/
    Wire.setSCL(GPIO_PIN_SCL);
    Wire.begin();
    EEPROM.begin();
#else
    EEPROM.begin(RESERVED_EEPROM_SIZE);
#endif
#endif /* NO_EEPROM */
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
#if !NO_EEPROM
    return EEPROM.read(address);
#else /* NO_EEPROM */
    return 0;
#endif /* NO_EEPROM */
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
#if !NO_EEPROM
    EEPROM.write(address, value);
#endif /* NO_EEPROM */
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
