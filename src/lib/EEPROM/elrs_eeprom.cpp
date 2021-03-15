#include "elrs_eeprom.h"
#include "../../src/targets.h"
#include <Arduino.h>

#if defined(PLATFORM_STM32) && TARGET_USE_EEPROM
    #if !defined(TARGET_EEPROM_ADDR)
        #define TARGET_EEPROM_ADDR 0x51
        #warning "!! Using default EEPROM address (0x51) !!"
    #endif

    #include <Wire.h>
    #include <extEEPROM.h>
    extEEPROM EEPROM(kbits_2, 1, 1, TARGET_EEPROM_ADDR);
#else
    #include <EEPROM.h>
#endif

void
ELRS_EEPROM::Begin()
{
#if defined(PLATFORM_STM32)
    #if TARGET_USE_EEPROM && \
            defined(GPIO_PIN_SDA) && (GPIO_PIN_SDA != UNDEF_PIN) && \
            defined(GPIO_PIN_SCL) && (GPIO_PIN_SCL != UNDEF_PIN)
        /* Initialize I2C */
        Wire.setSDA(GPIO_PIN_SDA);
        Wire.setSCL(GPIO_PIN_SCL);
        Wire.begin();
        /* Initialize EEPROM */
        EEPROM.begin(extEEPROM::twiClock100kHz, &Wire);
    #else // !TARGET_USE_EEPROM
        EEPROM.begin();
    #endif // TARGET_USE_EEPROM
#else /* !PLATFORM_STM32 */
    EEPROM.begin(RESERVED_EEPROM_SIZE);
#endif /* PLATFORM_STM32 */
}

uint8_t
ELRS_EEPROM::ReadByte(const uint32_t address)
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
ELRS_EEPROM::WriteByte(const uint32_t address, const uint8_t value)
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
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    if (!EEPROM.commit())
    {
      Serial.println("ERROR! EEPROM commit failed");
    }
#endif
}
