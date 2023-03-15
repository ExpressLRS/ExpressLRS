#include "config.h"
#include "config_legacy.h"
#include "common.h"
#include "POWERMGNT.h"
#include "OTA.h"
#include "helpers.h"
#include "logging.h"

#if defined(TARGET_TX)

#define MODEL_CHANGED       bit(1)
#define VTX_CHANGED         bit(2)
#define MAIN_CHANGED        bit(3) // catch-all for global config item
#define FAN_CHANGED         bit(4)
#define MOTION_CHANGED      bit(5)
#define BUTTON_CHANGED      bit(6)
#define ALL_CHANGED         (MODEL_CHANGED | VTX_CHANGED | MAIN_CHANGED | FAN_CHANGED | MOTION_CHANGED | BUTTON_CHANGED)

// Really awful but safe(?) type punning of model_config_t/v6_model_config_t to and from uint32_t
template<class T> static const void U32_to_Model(uint32_t const u32, T * const model)
{
    union {
        union {
            T model;
            uint8_t padding[sizeof(uint32_t)-sizeof(T)];
        } val;
        uint32_t u32;
    } converter = { .u32 = u32 };

    *model = converter.val.model;
}

template<class T> static const uint32_t Model_to_U32(T const * const model)
{
    // clear the entire union because the assignment will only fill sizeof(T)
    union {
        union {
            T model;
            uint8_t padding[sizeof(uint32_t)-sizeof(T)];
        } val;
        uint32_t u32;
    } converter = { 0 };

    converter.val.model = *model;
    return converter.u32;
}

static uint8_t RateV6toV7(uint8_t rateV6)
{
#if defined(RADIO_SX127X)
    if (rateV6 == 0)
    {
        // 200Hz stays same
        return 0;
    }

    // 100Hz, 50Hz, 25Hz all move up one
    // to make room for 100Hz Full
    return rateV6 + 1;
#else // RADIO_2400
    switch (rateV6)
    {
        case 0: return 4; // 500Hz
        case 1: return 6; // 250Hz
        case 2: return 7; // 150Hz
        case 3: return 9; // 50Hz
        default: return 4; // 500Hz
    }
#endif // RADIO_2400
}

static uint8_t RatioV6toV7(uint8_t ratioV6)
{
    // All shifted up for Std telem
    return ratioV6 + 1;
}

static uint8_t SwitchesV6toV7(uint8_t switchesV6)
{
    // 0 was removed, Wide(2) became 0, Hybrid(1) became 1
    switch (switchesV6)
    {
        case 1: return (uint8_t)smHybridOr16ch;
        case 2:
        default:
            return (uint8_t)smWideOr8ch;
    }
}

static void ModelV6toV7(v6_model_config_t const * const v6, model_config_t * const v7)
{
    v7->rate = RateV6toV7(v6->rate);
    v7->tlm = RatioV6toV7(v6->tlm);
    v7->power = v6->power;
    v7->switchMode = SwitchesV6toV7(v6->switchMode);
    v7->modelMatch = v6->modelMatch;
    v7->dynamicPower = v6->dynamicPower;
    v7->boostChannel = v6->boostChannel;
}

TxConfig::TxConfig() :
    m_model(m_config.model_config)
{
}

#if defined(PLATFORM_ESP32)
void TxConfig::Load()
{
    m_modified = 0;

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    ESP_ERROR_CHECK(nvs_open("ELRS", NVS_READWRITE, &handle));

    // Try to load the version and make sure it is a TX config
    uint32_t version = 0;
    if (nvs_get_u32(handle, "tx_version", &version) == ESP_OK && ((version & CONFIG_MAGIC_MASK) == TX_CONFIG_MAGIC))
        version = version & ~CONFIG_MAGIC_MASK;
    DBGLN("Config version %u", version);

    // Can't upgrade from version <5, or when flashing a previous version, just use defaults.
    if (version < 5 || version > TX_CONFIG_VERSION)
    {
        SetDefaults(true);
        return;
    }

    SetDefaults(false);

    uint32_t value;
    uint8_t value8;
    // vtx (v5)
    if (nvs_get_u32(handle, "vtx", &value) == ESP_OK)
    {
        m_config.vtxBand = value >> 24;
        m_config.vtxChannel = value >> 16;
        m_config.vtxPower = value >> 8;
        m_config.vtxPitmode = value;
    }

    // fanthresh (v5)
    if (nvs_get_u8(handle, "fanthresh", &value8) == ESP_OK)
        m_config.powerFanThreshold = value8;

    // Both of these were added to config v5 without incrementing the version
    if (nvs_get_u32(handle, "fan", &value) == ESP_OK)
        m_config.fanMode = value;
    if (nvs_get_u32(handle, "motion", &value) == ESP_OK)
        m_config.motionMode = value;

    if (version >= 6)
    {
        // dvr (v6)
        if (nvs_get_u8(handle, "dvraux", &value8) == ESP_OK)
            m_config.dvrAux = value8;
        if (nvs_get_u8(handle, "dvrstartdelay", &value8) == ESP_OK)
            m_config.dvrStartDelay = value8;
        if (nvs_get_u8(handle, "dvrstopdelay", &value8) == ESP_OK)
            m_config.dvrStopDelay = value8;
    }
    else
    {
        // Need to write the dvr defaults
        m_modified |= MAIN_CHANGED;
    }

    if (version >= 7) {
        // load button actions
        if (nvs_get_u32(handle, "button1", &value) == ESP_OK)
            m_config.buttonColors[0].raw = value;
        if (nvs_get_u32(handle, "button2", &value) == ESP_OK)
            m_config.buttonColors[1].raw = value;
        // backpackdisable was actually added after 7, but if not found will default to 0 (enabled)
        if (nvs_get_u8(handle, "backpackdisable", &value8) == ESP_OK)
            m_config.backpackDisable = value8;
    }

    for(unsigned i=0; i<64; i++)
    {
        char model[10] = "model";
        itoa(i, model+5, 10);
        if (nvs_get_u32(handle, model, &value) == ESP_OK)
        {
            if (version >= 7)
            {
                U32_to_Model(value, &m_config.model_config[i]);
            }
            else
            {
                // Upgrade v6 to v7 directly writing to nvs instead of calling Commit() over and over
                v6_model_config_t v6model;
                U32_to_Model(value, &v6model);
                model_config_t * const newModel = &m_config.model_config[i];
                ModelV6toV7(&v6model, newModel);
                nvs_set_u32(handle, model, Model_to_U32(newModel));
            }
        }
    } // for each model

    if (version != TX_CONFIG_VERSION)
    {
        Commit();
    }
}
#else  // STM32/ESP8266
void TxConfig::Load()
{
    m_modified = 0;
    m_eeprom->Get(0, m_config);

    uint32_t version = 0;
    if ((m_config.version & CONFIG_MAGIC_MASK) == TX_CONFIG_MAGIC)
        version = m_config.version & ~CONFIG_MAGIC_MASK;
    DBGLN("Config version %u", version);

    // If version is current, all done
    if (version == TX_CONFIG_VERSION)
        return;

    // Can't upgrade from version <5, or when flashing a previous version, just use defaults.
    if (version < 5 || version > TX_CONFIG_VERSION)
    {
        SetDefaults(true);
        return;
    }

    // Upgrade EEPROM, starting with defaults
    SetDefaults(false);

    if (version == 5)
    {
        UpgradeEepromV5ToV6();
        version = 6;
    }

    if (version == 6)
    {
        UpgradeEepromV6ToV7();
    }
}

void TxConfig::UpgradeEepromV5ToV6()
{
    v5_tx_config_t v5Config;
    v6_tx_config_t v6Config = { 0 }; // default the new fields to 0

    // Populate the prev version struct from eeprom
    m_eeprom->Get(0, v5Config);

    // Copy prev values to current config struct
    // This only workse because v5 and v6 are the same up to the new fields
    // which have already been set to 0
    memcpy(&v6Config, &v5Config, sizeof(v5Config));
    v6Config.version = 6U | TX_CONFIG_MAGIC;
    m_eeprom->Put(0, v6Config);
    m_eeprom->Commit();
}

void TxConfig::UpgradeEepromV6ToV7()
{
    v6_tx_config_t v6Config;

    // Populate the prev version struct from eeprom
    m_eeprom->Get(0, v6Config);

    // Manual field copying as some fields have moved
    #define LAZY(member) m_config.member = v6Config.member
    LAZY(vtxBand);
    LAZY(vtxChannel);
    LAZY(vtxPower);
    LAZY(vtxPitmode);
    LAZY(powerFanThreshold);
    LAZY(fanMode);
    LAZY(motionMode);
    LAZY(dvrAux);
    LAZY(dvrStartDelay);
    LAZY(dvrStopDelay);
    #undef LAZY

    for (unsigned i=0; i<64; i++)
    {
        ModelV6toV7(&v6Config.model_config[i], &m_config.model_config[i]);
    }

    m_modified = ALL_CHANGED;

    // Full Commit now
    m_config.version = 7U | TX_CONFIG_MAGIC;
    Commit();
}
#endif

void
TxConfig::Commit()
{
    if (!m_modified)
    {
        // No changes
        return;
    }
#if defined(PLATFORM_ESP32)
    // Write parts to NVS
    if (m_modified & MODEL_CHANGED)
    {
        uint32_t value = Model_to_U32(m_model);
        char model[10] = "model";
        itoa(m_modelId, model+5, 10);
        nvs_set_u32(handle, model, value);
    }
    if (m_modified & VTX_CHANGED)
    {
        uint32_t value =
            m_config.vtxBand << 24 |
            m_config.vtxChannel << 16 |
            m_config.vtxPower << 8 |
            m_config.vtxPitmode;
        nvs_set_u32(handle, "vtx", value);
    }
    if (m_modified & FAN_CHANGED)
    {
        uint32_t value = m_config.fanMode;
        nvs_set_u32(handle, "fan", value);
    }
    if (m_modified & MOTION_CHANGED)
    {
        uint32_t value = m_config.motionMode;
        nvs_set_u32(handle, "motion", value);
    }
    if (m_modified & MAIN_CHANGED)
    {
        nvs_set_u8(handle, "fanthresh", m_config.powerFanThreshold);

        nvs_set_u8(handle, "backpackdisable", m_config.backpackDisable);
        nvs_set_u8(handle, "dvraux", m_config.dvrAux);
        nvs_set_u8(handle, "dvrstartdelay", m_config.dvrStartDelay);
        nvs_set_u8(handle, "dvrstopdelay", m_config.dvrStopDelay);
    }
    if (m_modified & BUTTON_CHANGED)
    {
        nvs_set_u32(handle, "button1", m_config.buttonColors[0].raw);
        nvs_set_u32(handle, "button2", m_config.buttonColors[1].raw);
    }
    nvs_set_u32(handle, "tx_version", m_config.version);
    nvs_commit(handle);
#else
    // Write the struct to eeprom
    m_eeprom->Put(0, m_config);
    m_eeprom->Commit();
#endif
    m_modified = 0;
}

// Setters
void
TxConfig::SetRate(uint8_t rate)
{
    if (GetRate() != rate)
    {
        m_model->rate = rate;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetTlm(uint8_t tlm)
{
    if (GetTlm() != tlm)
    {
        m_model->tlm = tlm;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetPower(uint8_t power)
{
    if (GetPower() != power)
    {
        m_model->power = power;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetDynamicPower(bool dynamicPower)
{
    if (GetDynamicPower() != dynamicPower)
    {
        m_model->dynamicPower = dynamicPower;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetBoostChannel(uint8_t boostChannel)
{
    if (GetBoostChannel() != boostChannel)
    {
        m_model->boostChannel = boostChannel;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetSwitchMode(uint8_t switchMode)
{
    if (GetSwitchMode() != switchMode)
    {
        m_model->switchMode = switchMode;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetAntennaMode(uint8_t txAntenna)
{
    if (GetAntennaMode() != txAntenna)
    {
        m_model->txAntenna = txAntenna;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetModelMatch(bool modelMatch)
{
    if (GetModelMatch() != modelMatch)
    {
        m_model->modelMatch = modelMatch;
        m_modified |= MODEL_CHANGED;
    }
}

void
TxConfig::SetVtxBand(uint8_t vtxBand)
{
    if (m_config.vtxBand != vtxBand)
    {
        m_config.vtxBand = vtxBand;
        m_modified |= VTX_CHANGED;
    }
}

void
TxConfig::SetVtxChannel(uint8_t vtxChannel)
{
    if (m_config.vtxChannel != vtxChannel)
    {
        m_config.vtxChannel = vtxChannel;
        m_modified |= VTX_CHANGED;
    }
}

void
TxConfig::SetVtxPower(uint8_t vtxPower)
{
    if (m_config.vtxPower != vtxPower)
    {
        m_config.vtxPower = vtxPower;
        m_modified |= VTX_CHANGED;
    }
}

void
TxConfig::SetVtxPitmode(uint8_t vtxPitmode)
{
    if (m_config.vtxPitmode != vtxPitmode)
    {
        m_config.vtxPitmode = vtxPitmode;
        m_modified |= VTX_CHANGED;
    }
}

void
TxConfig::SetPowerFanThreshold(uint8_t powerFanThreshold)
{
    if (m_config.powerFanThreshold != powerFanThreshold)
    {
        m_config.powerFanThreshold = powerFanThreshold;
        m_modified |= MAIN_CHANGED;
    }
}

void
TxConfig::SetStorageProvider(ELRS_EEPROM *eeprom)
{
    if (eeprom)
    {
        m_eeprom = eeprom;
    }
}

void
TxConfig::SetFanMode(uint8_t fanMode)
{
    if (m_config.fanMode != fanMode)
    {
        m_config.fanMode = fanMode;
        m_modified |= FAN_CHANGED;
    }
}

void
TxConfig::SetMotionMode(uint8_t motionMode)
{
    if (m_config.motionMode != motionMode)
    {
        m_config.motionMode = motionMode;
        m_modified |= MOTION_CHANGED;
    }
}

void
TxConfig::SetDvrAux(uint8_t dvrAux)
{
    if (GetDvrAux() != dvrAux)
    {
        m_config.dvrAux = dvrAux;
        m_modified |= MAIN_CHANGED;
    }
}

void
TxConfig::SetDvrStartDelay(uint8_t dvrStartDelay)
{
    if (GetDvrStartDelay() != dvrStartDelay)
    {
        m_config.dvrStartDelay = dvrStartDelay;
        m_modified |= MAIN_CHANGED;
    }
}

void
TxConfig::SetDvrStopDelay(uint8_t dvrStopDelay)
{
    if (GetDvrStopDelay() != dvrStopDelay)
    {
        m_config.dvrStopDelay = dvrStopDelay;
        m_modified |= MAIN_CHANGED;
    }
}

void
TxConfig::SetBackpackDisable(bool backpackDisable)
{
    if (m_config.backpackDisable != backpackDisable)
    {
        m_config.backpackDisable = backpackDisable;
        m_modified |= MAIN_CHANGED;
    }
}

void
TxConfig::SetButtonActions(uint8_t button, tx_button_color_t *action)
{
    if (m_config.buttonColors[button].raw != action->raw) {
        m_config.buttonColors[button].raw = action->raw;
        m_modified |= BUTTON_CHANGED;
    }
}

void
TxConfig::SetDefaults(bool commit)
{
    // Reset everything to 0/false and then just set anything that zero is not appropriate
    memset(&m_config, 0, sizeof(m_config));

    m_config.version = TX_CONFIG_VERSION | TX_CONFIG_MAGIC;
    m_config.powerFanThreshold = PWR_250mW;
    m_modified = ALL_CHANGED;

    if (commit)
    {
        m_modified = ALL_CHANGED;
    }

    // Set defaults for button 1
    tx_button_color_t default_actions1 = {
        .val = {
            .color = 226,   // R:255 G:0 B:182
            .actions = {
                {false, 2, ACTION_BIND},
                {true, 0, ACTION_INCREASE_POWER}
            }
        }
    };
    m_config.buttonColors[0].raw = default_actions1.raw;

    // Set defaults for button 2
    tx_button_color_t default_actions2 = {
        .val = {
            .color = 3,     // R:0 G:0 B:255
            .actions = {
                {false, 1, ACTION_GOTO_VTX_CHANNEL},
                {true, 0, ACTION_SEND_VTX}
            }
        }
    };
    m_config.buttonColors[1].raw = default_actions2.raw;

    for (unsigned i=0; i<64; i++)
    {
        SetModelId(i);
        #if defined(RADIO_SX127X)
            SetRate(enumRatetoIndex(RATE_LORA_200HZ));
        #elif defined(RADIO_SX128X)
            SetRate(enumRatetoIndex(RATE_LORA_250HZ));
        #endif
        SetPower(POWERMGNT::getDefaultPower());
#if defined(PLATFORM_ESP32)
        // ESP32 nvs needs to commit every model
        if (commit)
        {
            m_modified |= MODEL_CHANGED;
            Commit();
        }
#endif
    }

#if !defined(PLATFORM_ESP32)
    // STM32/ESP8266 just needs one commit
    if (commit)
    {
        Commit();
    }
#endif

    SetModelId(0);
    m_modified = 0;
}

/**
 * Sets ModelId used for subsequent per-model config gets
 * Returns: true if the model has changed
 **/
bool
TxConfig::SetModelId(uint8_t modelId)
{
    model_config_t *newModel = &m_config.model_config[modelId];
    if (newModel != m_model)
    {
        m_model = newModel;
        m_modelId = modelId;
        return true;
    }

    return false;
}
#endif

/////////////////////////////////////////////////////

#if defined(TARGET_RX)

RxConfig::RxConfig()
{
}

void RxConfig::Load()
{
    m_modified = false;
    m_eeprom->Get(0, m_config);

    uint32_t version = 0;
    if ((m_config.version & CONFIG_MAGIC_MASK) == RX_CONFIG_MAGIC)
        version = m_config.version & ~CONFIG_MAGIC_MASK;
    DBGLN("Config version %u", version);

    // If version is current, all done
    if (version == RX_CONFIG_VERSION)
        return;

    // Can't upgrade from version <4, or when flashing a previous version, just use defaults.
    if (version < 4 || version > RX_CONFIG_VERSION)
    {
        SetDefaults(true);
        return;
    }

    // Upgrade EEPROM, starting with defaults
    SetDefaults(false);
    UpgradeEepromV4();
    UpgradeEepromV5();
    UpgradeEepromV6();
    m_config.version = RX_CONFIG_VERSION | RX_CONFIG_MAGIC;
    m_modified = true;
    Commit();
}

// ========================================================
// V4 Upgrade

static void PwmConfigV4(v4_rx_config_pwm_t const * const v4, rx_config_pwm_t * const current)
{
    current->val.failsafe = v4->val.failsafe;
    current->val.inputChannel = v4->val.inputChannel;
    current->val.inverted = v4->val.inverted;
}

void RxConfig::UpgradeEepromV4()
{
    v4_rx_config_t v4Config;
    m_eeprom->Get(0, v4Config);

    if ((v4Config.version & ~CONFIG_MAGIC_MASK) == 4)
    {
        m_config.isBound = v4Config.isBound;
        m_config.modelId = v4Config.modelId;
        memcpy(m_config.uid, v4Config.uid, sizeof(v4Config.uid));

        // OG PWMP had only 8 channels
        for (unsigned ch=0; ch<8; ++ch)
        {
            PwmConfigV4(&v4Config.pwmChannels[ch], &m_config.pwmChannels[ch]);
        }
    }
}

// ========================================================
// V5 Upgrade

static void PwmConfigV5(v5_rx_config_pwm_t const * const v5, rx_config_pwm_t * const current)
{
    current->val.failsafe = v5->val.failsafe;
    current->val.inputChannel = v5->val.inputChannel;
    current->val.inverted = v5->val.inverted;
    current->val.narrow = v5->val.narrow;
    current->val.mode = v5->val.mode;
    if (v5->val.mode > som400Hz)
    {
        current->val.mode += 1;
    }
}

void RxConfig::UpgradeEepromV5()
{
    v5_rx_config_t v5Config;
    m_eeprom->Get(0, v5Config);
    if ((v5Config.version & ~CONFIG_MAGIC_MASK) == 5)
    {
        memcpy(m_config.uid, v5Config.uid, sizeof(v5Config.uid));
        m_config.vbatScale = v5Config.vbatScale;
        m_config.isBound = v5Config.isBound;
        m_config.power = v5Config.power;
        m_config.antennaMode = v5Config.antennaMode;
        m_config.forceTlmOff = v5Config.forceTlmOff;
        m_config.rateInitialIdx = v5Config.rateInitialIdx;
        m_config.modelId = v5Config.modelId;

        for (unsigned ch=0; ch<16; ++ch)
        {
            PwmConfigV5(&v5Config.pwmChannels[ch], &m_config.pwmChannels[ch]);
        }
    }
}

// ========================================================
// V6 Upgrade
static void PwmConfigV6(v6_rx_config_pwm_t const * const v6, rx_config_pwm_t * const current)
{
    current->val.failsafe = v6->val.failsafe;
    current->val.inputChannel = v6->val.inputChannel;
    current->val.inverted = v6->val.inverted;
    current->val.narrow = v6->val.narrow;
    current->val.mode = v6->val.mode;
}

void RxConfig::UpgradeEepromV6()
{
    v6_rx_config_t v6Config;
    m_eeprom->Get(0, v6Config);
    if ((v6Config.version & ~CONFIG_MAGIC_MASK) == 6)
    {
        memcpy(m_config.uid, v6Config.uid, sizeof(v6Config.uid));
        m_config.vbatScale = v6Config.vbatScale;
        m_config.isBound = v6Config.isBound;
        m_config.power = v6Config.power;
        m_config.antennaMode = v6Config.antennaMode;
        m_config.forceTlmOff = v6Config.forceTlmOff;
        m_config.rateInitialIdx = v6Config.rateInitialIdx;
        m_config.modelId = v6Config.modelId;

        for (unsigned ch=0; ch<16; ++ch)
        {
            PwmConfigV6(&v6Config.pwmChannels[ch], &m_config.pwmChannels[ch]);
        }
    }
}

// ========================================================

void
RxConfig::Commit()
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
RxConfig::SetIsBound(bool isBound)
{
    if (m_config.isBound != isBound)
    {
        m_config.isBound = isBound;
        m_modified = true;
    }
}

void
RxConfig::SetUID(uint8_t* uid)
{
    for (uint8_t i = 0; i < UID_LEN; ++i)
    {
        m_config.uid[i] = uid[i];
    }
    m_modified = true;
}

void
RxConfig::SetOnLoan(bool isLoaned)
{
    if (m_config.onLoan != isLoaned)
    {
        m_config.onLoan = isLoaned;
        m_modified = true;
    }
}

void
RxConfig::SetOnLoanUID(uint8_t* uid)
{
    for (uint8_t i = 0; i < UID_LEN; ++i)
    {
        m_config.loanUID[i] = uid[i];
    }
    m_modified = true;
}

void
RxConfig::SetPowerOnCounter(uint8_t powerOnCounter)
{
    if (m_config.powerOnCounter != powerOnCounter)
    {
        m_config.powerOnCounter = powerOnCounter;
        m_modified = true;
    }
}

void
RxConfig::SetModelId(uint8_t modelId)
{
    if (m_config.modelId != modelId)
    {
        m_config.modelId = modelId;
        m_modified = true;
    }
}

void
RxConfig::SetPower(uint8_t power)
{
    if (m_config.power != power)
    {
        m_config.power = power;
        m_modified = true;
    }
}


void
RxConfig::SetAntennaMode(uint8_t antennaMode)
{
    //0 and 1 is use for gpio_antenna_select
    // 2 is diversity
    if (m_config.antennaMode != antennaMode)
    {
        m_config.antennaMode = antennaMode;
        m_modified = true;
    }
}

void
RxConfig::SetDefaults(bool commit)
{
    // Reset everything to 0/false and then just set anything that zero is not appropriate
    memset(&m_config, 0, sizeof(m_config));

    m_config.version = RX_CONFIG_VERSION | RX_CONFIG_MAGIC;
    m_config.modelId = 0xff;
    m_config.power = POWERMGNT::getDefaultPower();
    if (GPIO_PIN_ANT_CTRL != UNDEF_PIN)
        m_config.antennaMode = 2; // 2 is diversity
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
        m_config.antennaMode = 0; // 0 is diversity for dual radio

#if defined(GPIO_PIN_PWM_OUTPUTS)
    for (unsigned int ch=0; ch<PWM_MAX_CHANNELS; ++ch)
        SetPwmChannel(ch, 512, ch, false, 0, false);
    SetPwmChannel(2, 0, 2, false, 0, false); // ch2 is throttle, failsafe it to 988
#endif

#if defined(RCVR_INVERT_TX)
    m_config.serialProtocol = PROTOCOL_INVERTED_CRSF;
#else
    m_config.serialProtocol = PROTOCOL_CRSF;
#endif

    if (commit)
    {
        m_modified = true;
        Commit();
    }
}

void
RxConfig::SetStorageProvider(ELRS_EEPROM *eeprom)
{
    if (eeprom)
    {
        m_eeprom = eeprom;
    }
}

#if defined(GPIO_PIN_PWM_OUTPUTS)
void
RxConfig::SetPwmChannel(uint8_t ch, uint16_t failsafe, uint8_t inputCh, bool inverted, uint8_t mode, bool narrow)
{
    if (ch > PWM_MAX_CHANNELS)
        return;

    rx_config_pwm_t *pwm = &m_config.pwmChannels[ch];
    rx_config_pwm_t newConfig;
    newConfig.val.failsafe = failsafe;
    newConfig.val.inputChannel = inputCh;
    newConfig.val.inverted = inverted;
    newConfig.val.mode = mode;
    newConfig.val.narrow = narrow;
    if (pwm->raw == newConfig.raw)
        return;

    pwm->raw = newConfig.raw;
    m_modified = true;
}

void
RxConfig::SetPwmChannelRaw(uint8_t ch, uint32_t raw)
{
    if (ch > PWM_MAX_CHANNELS)
        return;

    rx_config_pwm_t *pwm = &m_config.pwmChannels[ch];
    if (pwm->raw == raw)
        return;

    pwm->raw = raw;
    m_modified = true;
}
#endif

void
RxConfig::SetForceTlmOff(bool forceTlmOff)
{
    if (m_config.forceTlmOff != forceTlmOff)
    {
        m_config.forceTlmOff = forceTlmOff;
        m_modified = true;
    }
}

void
RxConfig::SetRateInitialIdx(uint8_t rateInitialIdx)
{
    if (m_config.rateInitialIdx != rateInitialIdx)
    {
        m_config.rateInitialIdx = rateInitialIdx;
        m_modified = true;
    }
}

void RxConfig::SetSerialProtocol(eSerialProtocol serialProtocol)
{
    if (m_config.serialProtocol != serialProtocol)
    {
        m_config.serialProtocol = serialProtocol;
        m_modified = true;
    }
}
#endif
