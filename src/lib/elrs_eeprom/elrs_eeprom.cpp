#include "elrs_eeprom.h"
#include "targets.h"
#include "logging.h"

#if !defined(TARGET_NATIVE)
#if defined(PLATFORM_STM32)
    #if defined(TARGET_USE_EEPROM) && defined(USE_I2C)
        #if !defined(TARGET_EEPROM_ADDR)
            #define TARGET_EEPROM_ADDR 0x51
            #warning "!! Using default EEPROM address (0x51) !!"
        #endif

        #include <Wire.h>
        #include <extEEPROM.h>
        extEEPROM EEPROM(kbits_2, 1, 1, TARGET_EEPROM_ADDR);
    #else
        #define STM32_USE_FLASH
        #include <utility/stm32_eeprom.h>
    #endif
#else
    #include <EEPROM.h>
#endif

void
ELRS_EEPROM::Begin()
{
#if defined(PLATFORM_STM32)
    #if defined(STM32_USE_FLASH)
        eeprom_buffer_fill();
    #else // !STM32_USE_FLASH
        // I2C initialization is the responsibility of the caller
        // e.g. Wire.begin(GPIO_PIN_SDA, GPIO_PIN_SCL);

        /* Initialize EEPROM */
        #if defined(TARGET_EEPROM_400K)
            EEPROM.begin(extEEPROM::twiClock400kHz, &Wire);
        #else
            EEPROM.begin(extEEPROM::twiClock100kHz, &Wire);
        #endif
    #endif // STM32_USE_FLASH
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
        ERRLN("EEPROM address is out of bounds");
        return 0;
    }
#if defined(STM32_USE_FLASH)
    return eeprom_buffered_read_byte(address);
#else
    return EEPROM.read(address);
#endif
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
#if defined(STM32_USE_FLASH)
    eeprom_buffered_write_byte(address, value);
#elif defined(PLATFORM_STM32)
    EEPROM.update(address, value);
#else
    EEPROM.write(address, value);
#endif
}

void
ELRS_EEPROM::Commit()
{
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    if (!EEPROM.commit())
    {
      ERRLN("EEPROM commit failed");
    }
#elif defined(STM32_USE_FLASH)
    eeprom_buffer_flush();
#endif
  // PLATFORM_STM32 with external flash every byte is committed as it is written
}

#endif /* !TARGET_NATIVE */