#pragma once 

#include <Arduino.h>
#include "targets.h"

#define CONFIG_VERSION          1
#define RESERVED_EEPROM_SIZE    512

typedef struct {
    uint32_t    version;
    uint8_t     uid[6];
    uint32_t    rate;
    uint32_t    tlm;
    uint32_t    power;
    bool        modified;
} config_t;

class Config
{
public:
    Config();
    void Load();
    void Commit();

    // Getters
    uint32_t GetRate();
    uint32_t GetTlm();
    uint32_t GetPower();
    bool     IsModified();

    // Setters
    void ICACHE_RAM_ATTR SetRate(uint32_t rate);
    void ICACHE_RAM_ATTR SetTlm(uint32_t tlm);
    void ICACHE_RAM_ATTR SetPower(uint32_t power);
    void SetDefaults();

private:
    config_t    m_config;
    bool        m_isLoaded;
};
