#pragma once

#include "elrs_eeprom.h"

// CONFIG_MAGIC is ORed with CONFIG_VERSION in the version field
#define TX_BACKPACK_CONFIG_MAGIC    (0b01 << 30)
#define VRX_BACKPACK_CONFIG_MAGIC   (0b10 << 30)

#define TX_BACKPACK_CONFIG_VERSION      1
#define VRX_BACKPACK_CONFIG_VERSION     1

#if defined(TARGET_TX_BACKPACK)
typedef struct {
    uint32_t    version;
} tx_backpack_config_t;

class TxBackpackConfig
{
public:
    void Load();
    void Commit();

    // Getters
    bool     IsModified() const { return m_modified; }
    
    // Setters
    void SetStorageProvider(ELRS_EEPROM *eeprom);
    void SetDefaults();

private:    
    tx_backpack_config_t    m_config;
    ELRS_EEPROM             *m_eeprom;
    uint8_t                 m_modified;
};

extern TxBackpackConfig config;

#endif

///////////////////////////////////////////////////

#if defined(TARGET_VRX_BACKPACK)
typedef struct {
    uint32_t    version;
} vrx_backpack_config_t;

class VrxBackpackConfig
{
public:
    void Load();
    void Commit();

    // Getters
    bool     IsModified() const { return m_modified; }

    // Setters
    void SetStorageProvider(ELRS_EEPROM *eeprom);
    void SetDefaults();

private:
    vrx_backpack_config_t   m_config;
    ELRS_EEPROM             *m_eeprom;
    bool                    m_modified;
};

extern VrxBackpackConfig config;

#endif
