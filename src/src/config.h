#pragma once 

#include <Arduino.h>
#include "targets.h"
#include "elrs_eeprom.h"

#define CONFIG_VERSION          1
#define RESERVED_EEPROM_SIZE    512

typedef struct {
    uint32_t    version;
    uint32_t    rate;
    uint32_t    tlm;
    uint32_t    power;
    bool        modified;
} config_t;

class Config
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
    config_t    m_config;
    ELRS_EEPROM *m_eeprom;
};
