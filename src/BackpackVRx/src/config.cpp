#include "config.h"
#include "logging.h"

#if defined(TARGET_TX_BACKPACK)

void
TxBackpackConfig::Load()
{
    m_eeprom->Get(0, m_config);
    m_modified = 0;

    // Check if version number matches
    if (m_config.version != (uint32_t)(TX_BACKPACK_CONFIG_VERSION | TX_BACKPACK_CONFIG_MAGIC))
    {
        // If not, revert to defaults for this version
        DBGLN("EEPROM version mismatch! Resetting to defaults...");
        SetDefaults();
        Commit();
    }
}

void
TxBackpackConfig::Commit()
{
    if (!m_modified)
    {
        // No changes
        return;
    }
    // Write the struct to eeprom
    m_eeprom->Put(0, m_config);
    m_eeprom->Commit();

    m_modified = 0;
}

// Setters
void
TxBackpackConfig::SetStorageProvider(ELRS_EEPROM *eeprom)
{
    if (eeprom)
    {
        m_eeprom = eeprom;
    }
}

void
TxBackpackConfig::SetDefaults()
{
    m_config.version = TX_BACKPACK_CONFIG_VERSION | TX_BACKPACK_CONFIG_MAGIC;
    Commit();
}

#endif

/////////////////////////////////////////////////////

#if defined(TARGET_VRX_BACKPACK)

void
VrxBackpackConfig::Load()
{
    // Populate the struct from eeprom
    m_eeprom->Get(0, m_config);

    // Check if version number matches
    if (m_config.version != (uint32_t)(VRX_BACKPACK_CONFIG_VERSION | VRX_BACKPACK_CONFIG_MAGIC))
    {
        // If not, revert to defaults for this version
        DBGLN("EEPROM version mismatch! Resetting to defaults...");
        SetDefaults();
    }

    m_modified = false;
}

void
VrxBackpackConfig::Commit()
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
VrxBackpackConfig::SetStorageProvider(ELRS_EEPROM *eeprom)
{
    if (eeprom)
    {
        m_eeprom = eeprom;
    }
}

void
VrxBackpackConfig::SetDefaults()
{
    m_config.version = VRX_BACKPACK_CONFIG_VERSION | VRX_BACKPACK_CONFIG_MAGIC;
    Commit();
}

#endif
