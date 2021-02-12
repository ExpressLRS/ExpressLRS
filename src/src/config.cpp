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

    m_modified = false;
}

void
Config::Commit()
{    
    if (!m_modified)
    {
        // No changes
        return;
    }

    // Write the struct to eeprom
    m_eeprom->Put(0, m_config);
    m_eeprom->Commit();

    m_modified = false;
}
//luaxx
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
Config::GetSwitchMode()
{
    return m_config.switchMode;
}
uint32_t
Config::GetPower()
{
    return m_config.power;
}

bool
Config::IsModified()
{
    return m_modified;
}
//luaxx
// Setters
void
Config::SetRate(uint32_t rate)
{
    if (m_config.rate != rate)
    {
        m_config.rate = rate;
        m_modified = true;
    }
}

void
Config::SetTlm(uint32_t tlm)
{
    if (m_config.tlm != tlm)
    {
        m_config.tlm = tlm;
        m_modified = true;
    }
}

void
Config::SetSwitchMode(uint32_t modeSwitch)
{
    if (m_config.switchMode != modeSwitch)
    {
        m_config.switchMode = modeSwitch;
        m_modified = true;
    }
}
void
Config::SetPower(uint32_t power)
{
    if (m_config.power != power)
    {
        m_config.power = power;
        m_modified = true;
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
