#pragma once

#include "targets.h"
#include "elrs_eeprom.h"
#include "options.h"
#include "common.h"

#if defined(PLATFORM_ESP32)
#include <nvs_flash.h>
#include <nvs.h>
#endif

// CONFIG_MAGIC is ORed with CONFIG_VERSION in the version field
#define CONFIG_MAGIC_MASK   (0b11U << 30)
#define TX_CONFIG_MAGIC     (0b01U << 30)
#define RX_CONFIG_MAGIC     (0b10U << 30)

#define TX_CONFIG_VERSION   8U
#define RX_CONFIG_VERSION   10U

#if defined(TARGET_TX)

#define CONFIG_TX_BUTTON_ACTION_CNT 2
#define CONFIG_TX_MODEL_CNT         64

typedef enum {
    HT_OFF,
    HT_ON,
    HT_AUX1_UP,
    HT_AUX1_DN,
    HT_AUX2_UP,
    HT_AUX2_DN,
    HT_AUX3_UP,
    HT_AUX3_DN,
    HT_AUX4_UP,
    HT_AUX4_DN,
    HT_AUX5_UP,
    HT_AUX5_DN,
    HT_AUX6_UP,
    HT_AUX6_DN,
    HT_AUX7_UP,
    HT_AUX7_DN,
    HT_AUX8_UP,
    HT_AUX8_DN,
} headTrackingEnable_t;

typedef struct {
    uint32_t    rate:5,
                tlm:4,
                power:3,
                switchMode:2,
                boostChannel:3, // dynamic power boost AUX channel
                dynamicPower:1,
                modelMatch:1,
                txAntenna:2,    // FUTURE: Which TX antenna to use, 0=Auto
                ptrStartChannel:4,
                ptrEnableChannel:5,
                linkMode:2;
} model_config_t;

typedef struct {
    uint8_t     pressType:1,    // 0 short, 1 long
                count:3,        // 1-8 click count for short, .5sec hold count for long
                action:4;       // action to execute
} button_action_t;

typedef union {
    struct {
        uint8_t color;                  // RRRGGGBB
        button_action_t actions[CONFIG_TX_BUTTON_ACTION_CNT];
        uint8_t unused;
    } val;
    uint32_t raw;
} tx_button_color_t;

typedef enum {
    BACKPACK_TELEM_MODE_OFF,
    BACKPACK_TELEM_MODE_ESPNOW,
    BACKPACK_TELEM_MODE_WIFI,
    BACKPACK_TELEM_MODE_BLUETOOTH,
} telem_mode_t;

typedef struct {
    uint32_t        version;
    uint8_t         vtxBand;    // 0=Off, else band number
    uint8_t         vtxChannel; // 0=Ch1 -> 7=Ch8
    uint8_t         vtxPower;   // 0=Do not set, else power number
    uint8_t         vtxPitmode; // Off/On/AUX1^/AUX1v/etc
    uint8_t         powerFanThreshold:4; // Power level to enable fan if present
    model_config_t  model_config[CONFIG_TX_MODEL_CNT];
    uint8_t         fanMode;            // some value used by thermal?
    uint8_t         motionMode:2,       // bool, but space for 2 more modes
                    dvrStopDelay:3,
                    backpackDisable:1,  // bool, disable backpack via EN pin if available
                    backpackTlmMode:2;  // 0=Off, 1=Fwd tlm via espnow, 2=fwd tlm via wifi 3=(FUTURE) bluetooth
    uint8_t         dvrStartDelay:3,
                    dvrAux:5;
    tx_button_color_t buttonColors[2];  // FUTURE: TX RGB color / mode (sets color of TX, can be a static color or standard)
                                        // FUTURE: Model RGB color / mode (sets LED color mode on the model, but can be second TX led color too)
                                        // FUTURE: Custom button actions
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
    uint8_t GetAntennaMode() const { return m_model->txAntenna; }
    uint8_t GetLinkMode() const { return m_model->linkMode; }
    bool GetModelMatch() const { return m_model->modelMatch; }
    bool     IsModified() const { return m_modified; }
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
    bool     GetBackpackDisable() const { return m_config.backpackDisable; }
    uint8_t  GetBackpackTlmMode() const { return m_config.backpackTlmMode; }
    tx_button_color_t const *GetButtonActions(uint8_t button) const { return &m_config.buttonColors[button]; }
    model_config_t const &GetModelConfig(uint8_t model) const { return m_config.model_config[model]; }
    uint8_t GetPTRStartChannel() const { return m_model->ptrStartChannel; }
    uint8_t GetPTREnableChannel() const { return m_model->ptrEnableChannel; }

    // Setters
    void SetRate(uint8_t rate);
    void SetTlm(uint8_t tlm);
    void SetPower(uint8_t power);
    void SetDynamicPower(bool dynamicPower);
    void SetBoostChannel(uint8_t boostChannel);
    void SetSwitchMode(uint8_t switchMode);
    void SetAntennaMode(uint8_t txAntenna);
    void SetLinkMode(uint8_t linkMode);
    void SetModelMatch(bool modelMatch);
    void SetDefaults(bool commit);
    void SetStorageProvider(ELRS_EEPROM *eeprom);
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
    void SetButtonActions(uint8_t button, tx_button_color_t actions[2]);
    void SetBackpackDisable(bool backpackDisable);
    void SetBackpackTlmMode(uint8_t mode);
    void SetPTRStartChannel(uint8_t ptrStartChannel);
    void SetPTREnableChannel(uint8_t ptrEnableChannel);

    // State setters
    bool SetModelId(uint8_t modelId);

private:
#if !defined(PLATFORM_ESP32)
    void UpgradeEepromV5ToV6();
    void UpgradeEepromV6ToV7();
    void UpgradeEepromV7ToV8();
#endif

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
constexpr uint8_t PWM_MAX_CHANNELS = 16;

typedef enum : uint8_t {
    BINDSTORAGE_PERSISTENT = 0,
    BINDSTORAGE_VOLATILE = 1,
    BINDSTORAGE_RETURNABLE = 2,
} rx_config_bindstorage_t;

typedef union {
    struct {
        uint32_t failsafe:10,    // us output during failsafe +988 (e.g. 512 here would be 1500us)
                 inputChannel:4, // 0-based input channel
                 inverted:1,     // invert channel output
                 mode:4,         // Output mode (eServoOutputMode)
                 narrow:1,       // Narrow output mode (half pulse width)
                 failsafeMode:2, // failsafe output mode (eServoOutputFailsafeMode)
                 unused:10;      // FUTURE: When someone complains "everyone" uses inverted polarity PWM or something :/
    } val;
    uint32_t raw;
} rx_config_pwm_t;

typedef struct __attribute__((packed)) {
    uint32_t    version;
    uint8_t     uid[UID_LEN];
    uint8_t     unused_padding;
    uint8_t     serial1Protocol:4,  // secondary serial protocol
                serial1Protocol_unused:4;
    uint32_t    flash_discriminator;
    struct __attribute__((packed)) {
        uint16_t    scale;          // FUTURE: Override compiled vbat scale
        int16_t     offset;         // FUTURE: Override comiled vbat offset
    } vbat;
    uint8_t     bindStorage:2,     // rx_config_bindstorage_t
                power:4,
                antennaMode:2;      // 0=0, 1=1, 2=Diversity
    uint8_t     powerOnCounter:2,
                forceTlmOff:1,
                rateInitialIdx:5;   // Rate to start rateCycling at on boot
    uint8_t     modelId;
    uint8_t     serialProtocol:4,
                failsafeMode:2,
                unused:2;
    rx_config_pwm_t pwmChannels[PWM_MAX_CHANNELS] __attribute__((aligned(4)));
    uint8_t     teamraceChannel:4,
                teamracePosition:3,
                teamracePitMode:1;  // FUTURE: Enable pit mode when disabling model
    uint8_t     targetSysId;
    uint8_t     sourceSysId;
} rx_config_t;

class RxConfig
{
public:
    RxConfig();

    void Load();
    void Commit();

    // Getters
    bool     GetIsBound() const;
    const uint8_t* GetUID() const { return m_config.uid; }
#if defined(PLATFORM_ESP8266)
    uint8_t  GetPowerOnCounter() const;
#else
    uint8_t  GetPowerOnCounter() const { return m_config.powerOnCounter; }
#endif
    uint8_t  GetModelId() const { return m_config.modelId; }
    uint8_t GetPower() const { return m_config.power; }
    uint8_t GetAntennaMode() const { return m_config.antennaMode; }
    bool     IsModified() const { return m_modified; }
    #if defined(GPIO_PIN_PWM_OUTPUTS)
    const rx_config_pwm_t *GetPwmChannel(uint8_t ch) const { return &m_config.pwmChannels[ch]; }
    #endif
    bool GetForceTlmOff() const { return m_config.forceTlmOff; }
    uint8_t GetRateInitialIdx() const { return m_config.rateInitialIdx; }
    eSerialProtocol GetSerialProtocol() const { return (eSerialProtocol)m_config.serialProtocol; }
#if defined(PLATFORM_ESP32)
    eSerial1Protocol GetSerial1Protocol() const { return (eSerial1Protocol)m_config.serial1Protocol; }
#endif
    uint8_t GetTeamraceChannel() const { return m_config.teamraceChannel; }
    uint8_t GetTeamracePosition() const { return m_config.teamracePosition; }
    eFailsafeMode GetFailsafeMode() const { return (eFailsafeMode)m_config.failsafeMode; }
    uint8_t GetTargetSysId()  const { return m_config.targetSysId; }
    uint8_t GetSourceSysId()  const { return m_config.sourceSysId; }
    rx_config_bindstorage_t GetBindStorage() const { return (rx_config_bindstorage_t)m_config.bindStorage; }
    bool IsOnLoan() const;

    // Setters
    void SetUID(uint8_t* uid);
    void SetPowerOnCounter(uint8_t powerOnCounter);
    void SetModelId(uint8_t modelId);
    void SetPower(uint8_t power);
    void SetAntennaMode(uint8_t antennaMode);
    void SetDefaults(bool commit);
    void SetStorageProvider(ELRS_EEPROM *eeprom);
    #if defined(GPIO_PIN_PWM_OUTPUTS)
    void SetPwmChannel(uint8_t ch, uint16_t failsafe, uint8_t inputCh, bool inverted, uint8_t mode, bool narrow);
    void SetPwmChannelRaw(uint8_t ch, uint32_t raw);
    #endif
    void SetForceTlmOff(bool forceTlmOff);
    void SetRateInitialIdx(uint8_t rateInitialIdx);
    void SetSerialProtocol(eSerialProtocol serialProtocol);
#if defined(PLATFORM_ESP32)
    void SetSerial1Protocol(eSerial1Protocol serial1Protocol);
#endif
    void SetTeamraceChannel(uint8_t teamraceChannel);
    void SetTeamracePosition(uint8_t teamracePosition);
    void SetFailsafeMode(eFailsafeMode failsafeMode);
    void SetTargetSysId(uint8_t sysID);
    void SetSourceSysId(uint8_t sysID);
    void SetBindStorage(rx_config_bindstorage_t value);
    void ReturnLoan();

private:
    void CheckUpdateFlashedUid(bool skipDescrimCheck);
    void UpgradeUid(uint8_t *onLoanUid, uint8_t *boundUid);
    void UpgradeEepromV4();
    void UpgradeEepromV5();
    void UpgradeEepromV6();
    void UpgradeEepromV7V8();
    void UpgradeEepromV9();

    rx_config_t m_config;
    ELRS_EEPROM *m_eeprom;
    bool        m_modified;
};

extern RxConfig config;

#endif
