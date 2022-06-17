#include "config.h"
#include "config_legacy.h"
#include "common.h"
#include "POWERMGNT.h"
#include "OTA.h"
#include "logging.h"

#if defined(TARGET_TX)

#define MODEL_CHANGED       bit(1)
#define VTX_CHANGED         bit(2)
#define MAIN_CHANGED        bit(3) // catch-all for global config item
#define FAN_CHANGED         bit(4)
#define MOTION_CHANGED      bit(5)

TxConfig::TxConfig()
{
    SetDefaults(false);
}

void TxConfig::Load()
{
#if defined(PLATFORM_ESP32)
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    ESP_ERROR_CHECK(nvs_open("ELRS", NVS_READWRITE, &handle));

    // read version field
    if(nvs_get_u32(handle, "tx_version", &m_config.version) != ESP_ERR_NVS_NOT_FOUND
        && m_config.version == (TX_CONFIG_VERSION | TX_CONFIG_MAGIC))
    {
        DBGLN("Found version %u config", TX_CONFIG_VERSION);
        uint32_t value;
        nvs_get_u32(handle, "vtx", &value);
        m_config.vtxBand = value >> 24;
        m_config.vtxChannel = value >> 16;
        m_config.vtxPower = value >> 8;
        m_config.vtxPitmode = value;

        nvs_get_u32(handle, "fan", &value);
        m_config.fanMode = value;

        nvs_get_u32(handle, "motion", &value);
        m_config.motionMode = value;

        uint8_t value8;
        nvs_get_u8(handle, "fanthresh", &value8);
        m_config.powerFanThreshold = value8;

        nvs_get_u8(handle, "dvraux", &value8);
        m_config.dvrAux = value8;
        nvs_get_u8(handle, "dvrstartdelay", &value8);
        m_config.dvrStartDelay = value8;
        nvs_get_u8(handle, "dvrstopdelay", &value8);
        m_config.dvrStopDelay = value8;

        for(int i=0 ; i<64 ; i++)
        {
            char model[10] = "model";
            model_config_t value;
            itoa(i, model+5, 10);
            nvs_get_u32(handle, model, (uint32_t*)&value);
            m_config.model_config[i] = value;
        }
    }
    else
    {
        UpgradeEeprom();
    }
    m_modified = 0;
#else
    m_eeprom->Get(0, m_config);

    // Check if version number matches
    if (m_config.version != (TX_CONFIG_VERSION | TX_CONFIG_MAGIC))
    {
        UpgradeEeprom();
    }
    m_modified = 0;
#endif
}

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
        uint32_t value = *((uint32_t *)m_model);
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

        nvs_set_u8(handle, "dvraux", m_config.dvrAux);
        nvs_set_u8(handle, "dvrstartdelay", m_config.dvrStartDelay);
        nvs_set_u8(handle, "dvrstopdelay", m_config.dvrStopDelay);
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
TxConfig::SetDefaults(bool commit)
{
    expresslrs_mod_settings_s *const modParams = get_elrs_airRateConfig(RATE_DEFAULT);

    SetVtxBand(0);
    SetVtxChannel(0);
    SetVtxPower(0);
    SetVtxPitmode(0);
    SetPowerFanThreshold(PWR_250mW);
    SetFanMode(0);
    SetMotionMode(0);
    SetDvrAux(0);
    SetDvrStartDelay(0);
    SetDvrStopDelay(0);

    for (int i=0 ; i<64 ; i++) {
        SetModelId(i);
        SetRate(modParams->index);
        SetTlm(TLM_RATIO_STD);
        SetPower(POWERMGNT::getDefaultPower());
        SetDynamicPower(0);
        SetBoostChannel(0);
        SetSwitchMode((uint8_t)smWideOr8ch);
        SetModelMatch(false);
#if defined(PLATFORM_ESP32)
        // ESP32 nvs needs to commit every model
        if (commit)
            Commit();
#endif
    }

#if !defined(PLATFORM_ESP32)
    // STM32/ESP8266 just needs one commit
    if (commit)
        Commit();
#endif

    SetModelId(0);
}

void TxConfig::UpgradeEeprom()
{
    uint32_t startVersion = m_config.version & ~CONFIG_MAGIC_MASK;
    bool upgraded = false;

    // The upgraders must call Commit() or do their own committing
    if (m_config.version == (5U | TX_CONFIG_MAGIC))
    {
        UpgradeEepromV5ToV6();
        upgraded = true;
    }
    if (m_config.version == (6U | TX_CONFIG_MAGIC))
    {
        UpgradeEepromV6ToV7();
        upgraded = true;
    }

    if (upgraded)
    {
        DBGLN("EEPROM version %u upgraded to %u", startVersion, TX_CONFIG_VERSION);
    }
    else
    {
        DBGLN("EEPROM version %u could not be upgraded, using defaults", startVersion);
    }
}

void TxConfig::UpgradeEepromV5ToV6()
{
    // Always succeeds because this is guaranteed to be v5 by the caller
    m_config.version = 6U | TX_CONFIG_MAGIC;

#if defined(PLATFORM_ESP32)
    // Nothing is loaded here, assumes UpgradeEepromV6ToV7 will do the actual loading
    // Because the format of v5 and v6 is the same on ESP32 except for the extra fields

    // dvrAux = 0, dvrStartDelay = 0, dvrStopDelay = 0 are included in MAIN_CHANGED
    // force write ONLY the default DVR AUX settings and version (m_modified not ORed)
    m_modified = MAIN_CHANGED;

    Commit();
#else // STM32/ESP8266
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
#endif // STM32/ESP8266
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

void TxConfig::UpgradeEepromV6ToV7()
{
    // Always succeeds because this is guaranteed to be v6 by the caller
    m_config.version = 7U | TX_CONFIG_MAGIC;

#if defined(PLATFORM_ESP32)
    // V5 fields
    uint32_t value;
    nvs_get_u32(handle, "vtx", &value);
    m_config.vtxBand = value >> 24;
    m_config.vtxChannel = value >> 16;
    m_config.vtxPower = value >> 8;
    m_config.vtxPitmode = value;

    nvs_get_u32(handle, "fan", &value);
    m_config.fanMode = value;

    nvs_get_u32(handle, "motion", &value);
    m_config.motionMode = value;

    uint8_t value8;
    nvs_get_u8(handle, "fanthresh", &value8);
    m_config.powerFanThreshold = value8;

    // V6 fields (may not be there if this was an upgrade from V5)
    if (nvs_get_u8(handle, "dvraux", &value8) == ESP_OK)
        m_config.dvrAux = value8;
    if (nvs_get_u8(handle, "dvrstartdelay", &value8) == ESP_OK)
        m_config.dvrStartDelay = value8;
    if (nvs_get_u8(handle, "dvrstopdelay", &value8) == ESP_OK)
        m_config.dvrStopDelay = value8;

    // Model fields
    for(unsigned i=0; i<64; i++)
    {
        char model[10] = "model";
        itoa(i, model+5, 10);
        // Do a straight conversion with a direct read/write
        // instead of calling Commit() for every model
        v6_model_config_t v6model;
        nvs_get_u32(handle, model, (uint32_t*)&v6model);
        model_config_t *newModel = &m_config.model_config[i];
        ModelV6toV7(&v6model, newModel);
        uint32_t value32 = *((uint32_t *)newModel);
        nvs_set_u32(handle, model, value32);
    }
#else // STM32/ESP8266
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

    for(unsigned i=0; i<64; i++)
    {
        ModelV6toV7(&v6Config.model_config[i], &m_config.model_config[i]);
    }

#endif // STM32/ESP8266

    // Just write MAIN for ESP32, and STM32 always writes everything if anything changed
    m_modified = MAIN_CHANGED; // not MODEL_CHANGED, VTX_CHANGED, FAN_CHANGED, MOTION_CHANGED

    // Full Commit now
    Commit();
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
    SetDefaults(false);
}

void
RxConfig::Load()
{
    // Populate the struct from eeprom
    m_eeprom->Get(0, m_config);

    // Check if version number matches
    if (m_config.version != (RX_CONFIG_VERSION | RX_CONFIG_MAGIC))
    {
        UpgradeEeprom();
    }

    m_modified = false;
}

void RxConfig::UpgradeEeprom()
{
    uint32_t startVersion = m_config.version & ~CONFIG_MAGIC_MASK;

    // The upgraders must call Commit() or do their own committing
    if (m_config.version == (4U | RX_CONFIG_MAGIC))
    {
        UpgradeEepromV4ToV5();
        DBGLN("EEPROM version %u upgraded to %u", startVersion, RX_CONFIG_VERSION);
    }
    else
    {
        DBGLN("EEPROM version %u could not be upgraded, using defaults", startVersion);
    }
}

static void PwmConfigV4toV5(v4_rx_config_pwm_t const * const v4, rx_config_pwm_t * const v5)
{
    v5->val.failsafe = v4->val.failsafe;
    v5->val.inputChannel = v4->val.inputChannel;
    v5->val.inverted = v4->val.inverted;
}

void RxConfig::UpgradeEepromV4ToV5()
{
    v4_rx_config_t v4Config;
    m_eeprom->Get(0, v4Config);

    m_config.isBound = v4Config.isBound;
    m_config.modelId = v4Config.modelId;
    memcpy(m_config.uid, v4Config.uid, sizeof(v4Config.uid));

    // OG PWMP had only 8 channels
    for (unsigned ch=0; ch<8; ++ch)
    {
        PwmConfigV4toV5(&v4Config.pwmChannels[ch], &m_config.pwmChannels[ch]);
    }

    m_config.version = (5U | RX_CONFIG_MAGIC);
    m_modified = true;
    Commit();
}

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
    SetIsBound(false);
    SetPowerOnCounter(0);
    SetModelId(0xFF);
    SetPower(POWERMGNT::getDefaultPower());
    if (GPIO_PIN_ANTENNA_SELECT != UNDEF_PIN)
    {
        SetAntennaMode(2); //2 is diversity
    }
    else
    {
        SetAntennaMode(1); //0 and 1 is use for gpio_antenna_select
    }
#if defined(GPIO_PIN_PWM_OUTPUTS)
    for (unsigned int ch=0; ch<PWM_MAX_CHANNELS; ++ch)
        SetPwmChannel(ch, 512, ch, false, 0, false);
    SetPwmChannel(2, 0, 2, false, 0, false); // ch2 is throttle, failsafe it to 988
#endif
    SetOnLoan(false);

    if (commit)
    {
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

#endif
