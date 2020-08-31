#include "config.h"
#include "common.h"
#include "POWERMGNT.h"

void
Config::Load()
{
    // Populate the struct from eeprom
    m_eeprom->Get(0, m_config);

    // Check if version number matches
    if (m_config.version != CONFIG_VERSION)
    {
        // If not, revert to defaults for this version
        Serial.println("EEPROM version mismatch! Resetting to defaults...");
        SetDefaults();
    }

    m_config.modified = false;
}

void
Config::Commit()
{    
    if (!m_config.modified)
    {
        // No changes
        return;
    }

    // Write the struct to eeprom
    m_eeprom->Put(0, m_config);
    m_eeprom->Commit();

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

void
Config::SetStorageProvider(ELRS_EEPROM *eeprom)
{
    if (eeprom)
    {
        m_eeprom = eeprom;
    }
}
