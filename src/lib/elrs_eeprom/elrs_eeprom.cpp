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
#if defined(PLATFORM_ESP8266)
    // EEPROM.commit() erases a flash sector, and the flash window is unmapped for the
    // duration. Any interrupt that reaches code living in IROM therefore fetches garbage
    // and dies with Exception(0) IllegalInstruction, abandoning the erase and leaving the
    // config sector invalid -- which zeroes the UID and drops the receiver into binding
    // mode. The ISRs themselves are IRAM, but callees such as LQCALC's accessors are not:
    // GCC silently drops the section attribute for those COMDAT template members, so they
    // link into .text/IROM. Observed faulting in LQCALC<100>::currentIsSet() and in
    // hardware_pin(), i.e. more than one callee is affected, so mask here rather than
    // chase individual functions. A sector erase is tens of milliseconds and every caller
    // has already stopped the link, so blocking interrupts across it is safe.
    noInterrupts();
#endif
    const bool ok = EEPROM.commit();
#if defined(PLATFORM_ESP8266)
    interrupts();
#endif
    if (!ok)
    {
      ERRLN("EEPROM commit failed");
    }
}

#endif /* !TARGET_NATIVE */