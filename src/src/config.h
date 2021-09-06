#pragma once

#include "targets.h"
#include "elrs_eeprom.h"

// CONFIG_MAGIC is ORed with CONFIG_VERSION in the version field
#define TX_CONFIG_MAGIC     (0b01 << 30)
#define RX_CONFIG_MAGIC     (0b10 << 30)

#define TX_CONFIG_VERSION   4
#define RX_CONFIG_VERSION   3
#define UID_LEN             6

typedef struct {
    uint8_t     rate:3;
    uint8_t     tlm:3;
    uint8_t     power:3;
    uint8_t     switchMode:2;
    uint8_t     modelMatch:1;
    uint8_t     dynamicPower:1;
    uint8_t     boostChannel:3;
} model_config_t;

typedef struct {
    uint32_t        version;
    char            ssid[33];
    char            password[33];
    uint8_t         vtxBand;
    uint8_t         vtxChannel;
    uint8_t         vtxPower;
    uint8_t         vtxPitmode;
    model_config_t  model_config[64];
} tx_config_t;

class TxConfig
{
public:
    TxConfig() { SetModelId(0); }
    void Load();
    void Commit();

    // Getters
    uint8_t GetRate() const { return m_model->rate; }
    uint8_t GetTlm() const { return m_model->tlm; }
    uint8_t GetPower() const { return m_model->power; }
    bool GetDynamicPower() const { return m_model->dynamicPower; }
    uint8_t GetBoostChannel() const { return m_model->boostChannel; }
    uint8_t GetSwitchMode() const { return m_model->switchMode; }
    bool GetModelMatch() const { return m_model->modelMatch; }
    bool     IsModified() const { return m_modified; }
    const char* GetSSID() const { return m_config.ssid; }
    const char* GetPassword() const { return m_config.password; }
    uint8_t  GetVtxBand() const { return m_config.vtxBand; }
    uint8_t  GetVtxChannel() const { return m_config.vtxChannel; }
    uint8_t  GetVtxPower() const { return m_config.vtxPower; }
    uint8_t  GetVtxPitmode() const { return m_config.vtxPitmode; }

    // Setters
    void SetRate(uint8_t rate);
    void SetTlm(uint8_t tlm);
    void SetPower(uint8_t power);
    void SetDynamicPower(bool dynamicPower);
    void SetBoostChannel(uint8_t boostChannel);
    void SetSwitchMode(uint8_t switchMode);
    void SetModelMatch(bool modelMatch);
    void SetDefaults();
    void SetStorageProvider(ELRS_EEPROM *eeprom);
    void SetSSID(const char *ssid);
    void SetPassword(const char *password);
    void SetVtxBand(uint8_t vtxBand);
    void SetVtxChannel(uint8_t vtxChannel);
    void SetVtxPower(uint8_t vtxPower);
    void SetVtxPitmode(uint8_t vtxPitmode);

    // State setters
    bool SetModelId(uint8_t modelId);

private:
    tx_config_t m_config;
    ELRS_EEPROM *m_eeprom;
    bool        m_modified;
    model_config_t *m_model;
};

///////////////////////////////////////////////////

typedef struct {
    uint32_t    version;
    bool        isBound;
    uint8_t     uid[UID_LEN];
    uint8_t     powerOnCounter;
    uint8_t     modelId;
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
    uint8_t  GetModelId() const { return m_config.modelId; }
    bool     IsModified() const { return m_modified; }
    const char* GetSSID() const { return m_config.ssid; }
    const char* GetPassword() const { return m_config.password; }

    // Setters
    void SetIsBound(bool isBound);
    void SetUID(uint8_t* uid);
    void SetPowerOnCounter(uint8_t powerOnCounter);
    void SetModelId(uint8_t modelId);
    void SetDefaults();
    void SetStorageProvider(ELRS_EEPROM *eeprom);
    void SetSSID(const char *ssid);
    void SetPassword(const char *password);

private:
    rx_config_t m_config;
    ELRS_EEPROM *m_eeprom;
    bool        m_modified;
};
