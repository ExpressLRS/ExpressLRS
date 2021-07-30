#pragma once

#include "targets.h"
#include "elrs_eeprom.h"

extern const unsigned char target_name[];
extern const uint8_t target_name_size;

// CONFIG_MAGIC is ORed with CONFIG_VERSION in the version field
#define TX_CONFIG_MAGIC     (0b01 << 30)
#define RX_CONFIG_MAGIC     (0b10 << 30)

#define TX_CONFIG_VERSION   1
#define RX_CONFIG_VERSION   1
#define UID_LEN             6

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
    uint32_t GetRate() const { return m_config.rate; }
    uint32_t GetTlm() const { return m_config.tlm; }
    uint32_t GetPower() const { return m_config.power; }
    bool     IsModified() const { return m_modified; }

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
    uint8_t     uid[UID_LEN];
    uint8_t     powerOnCounter;
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

    // Setters
    void SetIsBound(bool isBound);
    void SetUID(uint8_t* uid);
    void SetPowerOnCounter(uint8_t powerOnCounter);
    void SetDefaults();
    void SetStorageProvider(ELRS_EEPROM *eeprom);

private:
    rx_config_t m_config;
    ELRS_EEPROM *m_eeprom;
    bool        m_modified;
};
