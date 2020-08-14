#include "config.h"
#include "common.h"
#include "POWERMGNT.h"

#include <Wire.h>
#ifdef PLATFORM_STM32
    #include <extEEPROM.h>
#else
    #include <EEPROM.h>
#endif

#ifdef PLATFORM_STM32
    extEEPROM EEPROM(kbits_2, 1, 1);
#endif

Config::Config()
: m_isLoaded(false)
{
}

void
Config::Load()
{
    // Init the eeprom
    #ifdef PLATFORM_STM32
        Wire.setSDA(GPIO_PIN_SDA); // set is needed or it wont work :/
        Wire.setSCL(GPIO_PIN_SCK);
        Wire.begin();
        EEPROM.begin();
    #else
        EEPROM.begin(RESERVED_EEPROM_SIZE);
    #endif

    // Populate the struct from eeprom
    #ifdef PLATFORM_ESP32
        EEPROM.get(0, m_config);
    #else
        int addr = 0;
        byte* p = (byte*)(void*)&m_config;
        byte i = sizeof(m_config);
        while (i--)
        {
            *p++ = EEPROM.read(addr++);
        }
    #endif

    // Check if version number matches
    if (m_config.version != CONFIG_VERSION)
    {
        // If not, revert to defaults for this version
        Serial.println("EEPROM version mismatch! Resetting to defaults...");
        SetDefaults();
    }

    m_config.modified = false;
    m_isLoaded = true;
}

void
Config::Commit()
{
    if (!m_isLoaded)
    {
        // We havent called Load() yet
        return;
    }
    
    if (!m_config.modified)
    {
        // No changes
        return;
    }

    // Write the struct to eeprom
    #ifdef PLATFORM_ESP32
        EEPROM.put(0, m_config);
        EEPROM.commit();
    #else
        int addr = 0;
        const byte* p = (const byte*)(const void*)&m_config;
        byte i = sizeof(m_config);
        while (i--)
        {
            EEPROM.write(addr++, *p++);
        }
    #endif

    m_config.modified = false;
}

// Getters
uint32_t
Config::GetRate()
{
    return m_config.rate;
}

uint32_t
Config::GetTlm()
{
    return m_config.tlm;
}

uint32_t
Config::GetPower()
{
    return m_config.power;
}

bool
Config::IsModified()
{
    return m_config.modified;
}

// Setters
void ICACHE_RAM_ATTR
Config::SetRate(uint32_t rate)
{
    if (m_config.rate != rate)
    {
        m_config.rate = rate;
        m_config.modified = true;
    }
}

void ICACHE_RAM_ATTR
Config::SetTlm(uint32_t tlm)
{
    if (m_config.tlm != tlm)
    {
        m_config.tlm = tlm;
        m_config.modified = true;
    }
}

void ICACHE_RAM_ATTR
Config::SetPower(uint32_t power)
{
    if (m_config.power != power)
    {
        m_config.power = power;
        m_config.modified = true;
    }
}

void
Config::SetDefaults()
{
    expresslrs_mod_settings_s *const modParams = get_elrs_airRateConfig(RATE_DEFAULT);
    m_config.version = CONFIG_VERSION;
    SetRate(modParams->index);
    SetTlm(modParams->TLMinterval);
    SetPower(DefaultPowerEnum);
    Commit();
}
