#include "config.h"
#include "common.h"
#include "POWERMGNT.h"

void
TxConfig::Load()
{
    // Populate the struct from eeprom
    m_eeprom->Get(0, m_config);

    // Check if version number matches
    if (m_config.version != TX_CONFIG_VERSION)
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
TxConfig::SetSwitchMode(uint32_t switchMode)
{
    if (m_config.switchMode != switchMode)
    {
        m_config.switchMode = switchMode;
        m_modified = true;
    }
}

void
TxConfig::SetDefaults()
{
    expresslrs_mod_settings_s *const modParams = get_elrs_airRateConfig(RATE_DEFAULT);
    m_config.version = TX_CONFIG_VERSION;
    SetRate(modParams->index);
    SetTlm(modParams->TLMinterval);
    SetPower(DefaultPowerEnum);
    SetSwitchMode(1);
    SetSSID("");
    SetPassword("");
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

void
TxConfig::SetSSID(const char *ssid)
{
    strncpy(m_config.ssid, ssid, sizeof(m_config.ssid)-1);
    m_modified = true;
}

void
TxConfig::SetPassword(const char *password)
{
    strncpy(m_config.password, password, sizeof(m_config.password)-1);
    m_modified = true;
}

/////////////////////////////////////////////////////

void
RxConfig::Load()
{
    // Populate the struct from eeprom
    m_eeprom->Get(0, m_config);

    // Check if version number matches
    if (m_config.version != RX_CONFIG_VERSION)
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

// Setters
void
RxConfig::SetIsBound(bool isBound)
{
    if (m_config.isBound != isBound)
    {
        m_config.isBound = isBound;
        m_modified = true;
    }
}

void
RxConfig::SetUID(uint8_t* uid)
{
    for (uint8_t i = 0; i < UID_LEN; ++i)
    {
        m_config.uid[i] = uid[i];
    }
    m_modified = true;
}

void
RxConfig::SetPowerOnCounter(uint8_t powerOnCounter)
{
    if (m_config.powerOnCounter != powerOnCounter)
    {
        m_config.powerOnCounter = powerOnCounter;
        m_modified = true;
    }
}

void
RxConfig::SetDefaults()
{
    m_config.version = RX_CONFIG_VERSION;
    SetIsBound(false);
    SetPowerOnCounter(0);
    SetSSID("");
    SetPassword("");
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

void
RxConfig::SetSSID(const char *ssid)
{
    strncpy(m_config.ssid, ssid, sizeof(m_config.ssid)-1);
    m_modified = true;
}

void
RxConfig::SetPassword(const char *password)
{
    strncpy(m_config.password, password, sizeof(m_config.password)-1);
    m_modified = true;
}
