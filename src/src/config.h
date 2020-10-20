#pragma once 

#include <Arduino.h>
#include "targets.h"
#include "elrs_eeprom.h"

#define CONFIG_VERSION          1

typedef struct {
    uint32_t    version;
    uint32_t    rate;
    uint32_t    tlm;
    uint32_t    power;
} tx_config_t;

class TxConfig
{
public:
    void Load();
    void Commit();

    // Getters
    uint32_t GetRate();
    uint32_t GetTlm();
    uint32_t GetPower();
    bool     IsModified();

    // Setters
    void SetRate(uint32_t rate);
    void SetTlm(uint32_t tlm);
    void SetPower(uint32_t power);
    void SetDefaults();
    void SetStorageProvider(ELRS_EEPROM *eeprom);

private:
    tx_config_t m_config;
    ELRS_EEPROM *m_eeprom;
    bool        m_modified;
};

///////////////////////////////////////////////////

typedef struct {
    uint32_t    version;
    bool        isBound;
    uint8_t     uid[6];
} rx_config_t;

class RxConfig
{
public:
    void Load();
    void Commit();

    // Getters
    bool     GetIsBound();
    uint8_t* GetUID();
    bool     IsModified();

    // Setters
    void SetIsBound(bool isBound);
    void SetUID(uint8_t* uid);
    void SetDefaults();
    void SetStorageProvider(ELRS_EEPROM *eeprom);

private:
    rx_config_t m_config;
    ELRS_EEPROM *m_eeprom;
    bool        m_modified;
};
