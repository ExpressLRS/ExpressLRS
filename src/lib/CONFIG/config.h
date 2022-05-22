#pragma once

#include "targets.h"
#include "elrs_eeprom.h"
#include "options.h"

#if defined(PLATFORM_ESP32)
#include <nvs_flash.h>
#include <nvs.h>
#endif

// CONFIG_MAGIC is ORed with CONFIG_VERSION in the version field
#define TX_CONFIG_MAGIC     (0b01 << 30)
#define RX_CONFIG_MAGIC     (0b10 << 30)

#define TX_CONFIG_VERSION   6
#define RX_CONFIG_VERSION   5
#define UID_LEN             6

#if defined(TARGET_TX)
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
    uint8_t         powerFanThreshold:4; // Power level to enable fan if present
    model_config_t  model_config[64];
    uint8_t         fanMode;
    uint8_t         motionMode;
    uint8_t         dvrAux:5;
    uint8_t         dvrStartDelay:3;
    uint8_t         dvrStopDelay:3;
} tx_config_t;

class TxConfig
{
public:
    TxConfig();
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
    uint8_t GetPowerFanThreshold() const { return m_config.powerFanThreshold; }
    uint8_t  GetFanMode() const { return m_config.fanMode; }
    uint8_t  GetMotionMode() const { return m_config.motionMode; }
    uint8_t  GetDvrAux() const { return m_config.dvrAux; }
    uint8_t  GetDvrStartDelay() const { return m_config.dvrStartDelay; }
    uint8_t  GetDvrStopDelay() const { return m_config.dvrStopDelay; }

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
    void SetPowerFanThreshold(uint8_t powerFanThreshold);
    void SetFanMode(uint8_t fanMode);
    void SetMotionMode(uint8_t motionMode);
    void SetDvrAux(uint8_t dvrAux);
    void SetDvrStartDelay(uint8_t dvrStartDelay);
    void SetDvrStopDelay(uint8_t dvrStopDelay);

    // State setters
    bool SetModelId(uint8_t modelId);

private:
    bool UpgradeEepromV5ToV6();

    tx_config_t m_config;
    ELRS_EEPROM *m_eeprom;
    uint8_t     m_modified;
    model_config_t *m_model;
    uint8_t     m_modelId;
#if defined(PLATFORM_ESP32)
    nvs_handle  handle;
#endif
};

extern TxConfig config;

#endif

///////////////////////////////////////////////////

#if defined(TARGET_RX)
constexpr uint8_t PWM_MAX_CHANNELS = 12;

typedef union {
    struct {
        unsigned failsafe:10;   // us output during failsafe +988 (e.g. 512 here would be 1500us)
        unsigned inputChannel:4; // 0-based input channel
        unsigned inverted:1;     // invert channel output
        unsigned mode:4;         // Output mode (eServoOutputMode)
        unsigned narrow:1;       // Narrow output mode (half pulse width)
        unsigned unused:13;
    } val;
    uint32_t raw;
} rx_config_pwm_t;

typedef struct {
    uint32_t    version;
    bool        isBound;
    uint8_t     uid[UID_LEN];
    bool        onLoan;
    uint8_t     loanUID[UID_LEN];
    uint8_t     powerOnCounter;
    uint8_t     modelId;
    uint8_t     power;
    uint8_t     antennaMode;    //keep antenna mode in struct even in non diversity RX,
                                // because turning feature diversity on and off would require change of RX config version.
    char        ssid[33];
    char        password[33];
    rx_config_pwm_t pwmChannels[PWM_MAX_CHANNELS];
} rx_config_t;

class RxConfig
{
public:
    void Load();
    void Commit();

    // Getters
    bool     GetIsBound() const { return firmwareOptions.hasUID || m_config.isBound; }
    const uint8_t* GetUID() const { return m_config.uid; }
    bool GetOnLoan() const { return m_config.onLoan; }
    const uint8_t* GetOnLoanUID() const { return m_config.loanUID; }
    uint8_t  GetPowerOnCounter() const { return m_config.powerOnCounter; }
    uint8_t  GetModelId() const { return m_config.modelId; }
    uint8_t GetPower() const { return m_config.power; }
    uint8_t GetAntennaMode() const { return m_config.antennaMode; }
    bool     IsModified() const { return m_modified; }
    const char* GetSSID() const { return m_config.ssid; }
    const char* GetPassword() const { return m_config.password; }
    #if defined(GPIO_PIN_PWM_OUTPUTS)
    const rx_config_pwm_t *GetPwmChannel(uint8_t ch) { return &m_config.pwmChannels[ch]; }
    #endif

    // Setters
    void SetIsBound(bool isBound);
    void SetUID(uint8_t* uid);
    void SetOnLoan(bool loaned);
    void SetOnLoanUID(uint8_t* uid);
    void SetPowerOnCounter(uint8_t powerOnCounter);
    void SetModelId(uint8_t modelId);
    void SetPower(uint8_t power);
    void SetAntennaMode(uint8_t antennaMode);
    void SetDefaults();
    void SetStorageProvider(ELRS_EEPROM *eeprom);
    void SetSSID(const char *ssid);
    void SetPassword(const char *password);
    #if defined(GPIO_PIN_PWM_OUTPUTS)
    void SetPwmChannel(uint8_t ch, uint16_t failsafe, uint8_t inputCh, bool inverted, uint8_t mode, bool narrow);
    void SetPwmChannelRaw(uint8_t ch, uint32_t raw);
    #endif

private:
    rx_config_t m_config;
    ELRS_EEPROM *m_eeprom;
    bool        m_modified;
};

extern RxConfig config;

#endif
