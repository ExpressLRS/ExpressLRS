#include "elrs_eeprom.h"
#include "targets.h"
#include "logging.h"

#if !defined(TARGET_NATIVE)
#include <EEPROM.h>

void
ELRS_EEPROM::Begin()
{
#if defined(PLATFORM_STM32)
    #if defined(STM32_USE_FLASH)
        eeprom_buffer_fill();
    #else // !STM32_USE_FLASH
        // I2C initialization is the responsibility of the caller
        // e.g. Wire.begin(GPIO_PIN_SDA, GPIO_PIN_SCL);
        DBGLN("Initializing EEPROM");
        /* Initialize EEPROM */
        #if defined(TARGET_EEPROM_400K)
            DBGLN("400K");
            EEPROM.begin(extEEPROM::twiClock400kHz);
        #else
            DBGLN("100K");
            EEPROM.begin(extEEPROM::twiClock100kHz);
        #endif
    #endif // STM32_USE_FLASH
#else /* !PLATFORM_STM32 */
    EEPROM.begin(RESERVED_EEPROM_SIZE);
#endif /* PLATFORM_STM32 */
DBGLN("EEPROM INIT");
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
#if !defined(PLATFORM_STM32)
    if (!EEPROM.commit())
    {
      ERRLN("EEPROM commit failed");
    }
#endif
}

#endif /* !TARGET_NATIVE */