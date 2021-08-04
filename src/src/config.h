#pragma once 

#include "targets.h"
#include "elrs_eeprom.h"

#define TX_CONFIG_VERSION   3
#define RX_CONFIG_VERSION   2
#define UID_LEN             6

typedef struct {
    uint8_t    rate:3;
    uint8_t    tlm:3;
    uint8_t    power:3;
    uint8_t    switchMode:2;
} model_config_t;

typedef struct {
    uint32_t        version;
    char            ssid[33];
    char            password[33];
    model_config_t  model_config[64];
} tx_config_t;

class TxConfig
{
public:
    void Load();
    void Commit();

    // Getters
    uint8_t GetRate(uint8_t modelId) const { return m_config.model_config[modelId].rate; }
    uint8_t GetTlm(uint8_t modelId) const { return m_config.model_config[modelId].tlm; }
    uint8_t GetPower(uint8_t modelId) const { return m_config.model_config[modelId].power; }
    uint8_t GetSwitchMode(uint8_t modelId) const { return m_config.model_config[modelId].switchMode; }
    bool     IsModified() const { return m_modified; }
    const char* GetSSID() const { return m_config.ssid; }
    const char* GetPassword() const { return m_config.password; }

    // Setters
    void SetRate(uint8_t modelId, uint8_t rate);
    void SetTlm(uint8_t modelId, uint8_t tlm);
    void SetPower(uint8_t modelId, uint8_t power);
    void SetSwitchMode(uint8_t modelId, uint8_t switchMode);
    void SetDefaults();
    void SetStorageProvider(ELRS_EEPROM *eeprom);
    void SetSSID(const char *ssid);
    void SetPassword(const char *password);

private:
    tx_config_t m_config;
    ELRS_EEPROM *m_eeprom;
    bool        m_modified;
};

///////////////////////////////////////////////////

typedef struct {
    uint32_t    version;
    bool        isBound;
    uint8_t     uid[UID_LEN];
    uint8_t     powerOnCounter;
    char        ssid[33];
    char        password[33];
} rx_config_t;

class RxConfig
{
public:
    void Load();
    void Commit();

    // Getters
    bool     GetIsBound() const {
        #ifdef MY_UID
            return true;
        #else
            return m_config.isBound;
        #endif
    }
    const uint8_t* GetUID() const { return m_config.uid; }
    uint8_t  GetPowerOnCounter() const { return m_config.powerOnCounter; }
    bool     IsModified() const { return m_modified; }
    const char* GetSSID() const { return m_config.ssid; }
    const char* GetPassword() const { return m_config.password; }

    // Setters
    void SetIsBound(bool isBound);
    void SetUID(uint8_t* uid);
    void SetPowerOnCounter(uint8_t powerOnCounter);
    void SetDefaults();
    void SetStorageProvider(ELRS_EEPROM *eeprom);
    void SetSSID(const char *ssid);
    void SetPassword(const char *password);

private:
    rx_config_t m_config;
    ELRS_EEPROM *m_eeprom;
    bool        m_modified;
};
