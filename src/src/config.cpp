#include "config.h"
#include "common.h"
#include "POWERMGNT.h"

void
TxConfig::Load()
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
TxConfig::Commit()
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

// Getters
uint32_t
TxConfig::GetRate()
{
    return m_config.rate;
}

uint32_t
TxConfig::GetTlm()
{
    return m_config.tlm;
}

uint32_t
TxConfig::GetPower()
{
    return m_config.power;
}

bool
TxConfig::IsModified()
{
    return m_modified;
}

// Setters
void
TxConfig::SetRate(uint32_t rate)
{
    if (m_config.rate != rate)
    {
        m_config.rate = rate;
        m_modified = true;
    }
}

void
TxConfig::SetTlm(uint32_t tlm)
{
    if (m_config.tlm != tlm)
    {
        m_config.tlm = tlm;
        m_modified = true;
    }
}

void
TxConfig::SetPower(uint32_t power)
{
    if (m_config.power != power)
    {
        m_config.power = power;
        m_modified = true;
    }
}

void
TxConfig::SetDefaults()
{
    expresslrs_mod_settings_s *const modParams = get_elrs_airRateConfig(RATE_DEFAULT);
    m_config.version = CONFIG_VERSION;
    SetRate(modParams->index);
    SetTlm(modParams->TLMinterval);
    SetPower(DefaultPowerEnum);
    Commit();
}

void
TxConfig::SetStorageProvider(ELRS_EEPROM *eeprom)
{
    if (eeprom)
    {
        m_eeprom = eeprom;
    }
}

/////////////////////////////////////////////////////

void
RxConfig::Load()
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
RxConfig::Commit()
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

// Getters
// uint32_t
// RxConfig::GetRate()
// {
//     return m_config.rate;
// }

bool
RxConfig::IsModified()
{
    return m_modified;
}

// Setters
// void
// RxConfig::SetRate(uint32_t rate)
// {
//     if (m_config.rate != rate)
//     {
//         m_config.rate = rate;
//         m_modified = true;
//     }
// }

void
RxConfig::SetDefaults()
{
    expresslrs_mod_settings_s *const modParams = get_elrs_airRateConfig(RATE_DEFAULT);
    m_config.version = CONFIG_VERSION;
    // SetRate(modParams->index);
    // SetTlm(modParams->TLMinterval);
    // SetPower(DefaultPowerEnum);
    Commit();
}

void
RxConfig::SetStorageProvider(ELRS_EEPROM *eeprom)
{
    if (eeprom)
    {
        m_eeprom = eeprom;
    }
}