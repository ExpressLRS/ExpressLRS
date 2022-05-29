#include "config.h"
#include "common.h"
#include "POWERMGNT.h"
#include "OTA.h"
#include "logging.h"

#if defined(TARGET_TX)

#define MODEL_CHANGED       bit(1)
#define VTX_CHANGED         bit(2)
#define SSID_CHANGED        bit(3)
#define PASSWORD_CHANGED    bit(4)
#define MAIN_CHANGED        bit(5) // catch-all for global config item
#define FAN_CHANGED         bit(6)
#define MOTION_CHANGED      bit(7)

TxConfig::TxConfig()
{
    SetModelId(0);
}

#if defined(PLATFORM_ESP32)
void
TxConfig::Load()
{
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
    uint32_t version;
    if(nvs_get_u32(handle, "tx_version", &version) != ESP_ERR_NVS_NOT_FOUND
        && version == (uint32_t)(TX_CONFIG_VERSION | TX_CONFIG_MAGIC))
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

        value = sizeof(m_config.ssid);
        nvs_get_str(handle, "ssid", m_config.ssid, &value);
        value = sizeof(m_config.password);
        nvs_get_str(handle, "password", m_config.password, &value);
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
        if(!UpgradeEepromV5ToV6())
        {
            // If not, revert to defaults for this version
            DBGLN("EEPROM version mismatch! Resetting to defaults...");
            SetDefaults();
        }
        nvs_set_u32(handle, "tx_version", TX_CONFIG_VERSION | TX_CONFIG_MAGIC);
        nvs_commit(handle);
    }
    m_model = &m_config.model_config[m_modelId];
    m_modified = 0;
}
#else
void
TxConfig::Load()
{
    m_eeprom->Get(0, m_config);
    m_modified = 0;

    // Check if version number matches
    if (m_config.version != (uint32_t)(TX_CONFIG_VERSION | TX_CONFIG_MAGIC))
    {
        // Check if we can update the config
        if (!UpgradeEepromV5ToV6())
        {
            // If not, revert to defaults for this version
            DBGLN("EEPROM version mismatch! Resetting to defaults...");
            SetDefaults();
            Commit();
        }
    }
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
    if (m_modified & SSID_CHANGED)
        nvs_set_str(handle, "ssid", m_config.ssid);
    if (m_modified & PASSWORD_CHANGED)
        nvs_set_str(handle, "password", m_config.password);
    if (m_modified & MAIN_CHANGED)
    {
        nvs_set_u8(handle, "fanthresh", m_config.powerFanThreshold);

        nvs_set_u8(handle, "dvraux", m_config.dvrAux);
        nvs_set_u8(handle, "dvrstartdelay", m_config.dvrStartDelay);
        nvs_set_u8(handle, "dvrstopdelay", m_config.dvrStopDelay);
    }
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
TxConfig::SetSSID(const char *ssid)
{
    strncpy(m_config.ssid, ssid, sizeof(m_config.ssid)-1);
    m_modified |= SSID_CHANGED;
}

void
TxConfig::SetPassword(const char *password)
{
    strncpy(m_config.password, password, sizeof(m_config.password)-1);
    m_modified |= PASSWORD_CHANGED;
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
TxConfig::SetDefaults()
{
    expresslrs_mod_settings_s *const modParams = get_elrs_airRateConfig(RATE_DEFAULT);
    m_config.version = TX_CONFIG_VERSION | TX_CONFIG_MAGIC;
    SetSSID("");
    SetPassword("");
    SetVtxBand(0);
    SetVtxChannel(0);
    SetVtxPower(0);
    SetVtxPitmode(0);
    SetPowerFanThreshold(PWR_250mW);
    SetDvrAux(0);
    SetDvrStartDelay(0);
    SetDvrStopDelay(0);
    Commit();
    for (int i=0 ; i<64 ; i++) {
        SetModelId(i);
        SetRate(modParams->index);
        SetTlm(TLM_RATIO_STD);
        SetPower(POWERMGNT::getDefaultPower());
        SetDynamicPower(0);
        SetBoostChannel(0);
        SetSwitchMode((uint8_t)smHybrid);
        SetModelMatch(false);
        SetFanMode(0);
        SetMotionMode(0);
        Commit();
    }

    SetModelId(0);
}

bool
TxConfig::UpgradeEepromV5ToV6()
{
    #define PREV_TX_CONFIG_VERSION 5

    #if defined(PLATFORM_ESP32)
        uint32_t version;
        if (nvs_get_u32(handle, "tx_version", &version) != ESP_ERR_NVS_NOT_FOUND
        && version != (uint32_t)(PREV_TX_CONFIG_VERSION | TX_CONFIG_MAGIC))
        {
            // Cannot support an upgrade from this version
            return false;
        }

        // Populate the named values that should exist from prev config
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

        value = sizeof(m_config.ssid);
        nvs_get_str(handle, "ssid", m_config.ssid, &value);
        value = sizeof(m_config.password);
        nvs_get_str(handle, "password", m_config.password, &value);
        uint8_t value8;
        nvs_get_u8(handle, "fanthresh", &value8);
        m_config.powerFanThreshold = value8;

        for(int i=0 ; i<64 ; i++)
        {
            char model[10] = "model";
            model_config_t value;
            itoa(i, model+5, 10);
            nvs_get_u32(handle, model, (uint32_t*)&value);
            m_config.model_config[i] = value;
        }
    #else // STM32
        // Define config struct based on the prev EEPROM schema
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
        } v5_tx_config_t;

        v5_tx_config_t v5Config;

        // Populate the prev version struct from eeprom
        m_eeprom->Get(0, v5Config);
        if (v5Config.version != (uint32_t)(PREV_TX_CONFIG_VERSION | TX_CONFIG_MAGIC))
        {
            // Cannot support an upgrade from this version
            return false;
        }

        // Copy prev values to current config struct
        memcpy(&m_config, &v5Config, sizeof(v5Config));
    #endif

    DBGLN("EEPROM version %u is out of date... upgrading to version %u", PREV_TX_CONFIG_VERSION, TX_CONFIG_VERSION);

    // Update the version param
    m_config.version = TX_CONFIG_VERSION | TX_CONFIG_MAGIC;

    // V6 config is the same as V5, with the addition of 3 new
    // fields in the tx_config_t struct. Set defaults for new
    // fields, and write changes to EEPROM
    SetDvrAux(0);
    SetDvrStartDelay(0);
    SetDvrStopDelay(0);
    m_modified |= MAIN_CHANGED; // force write the default DVR AUX settings

    Commit();

    return true;
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

void
RxConfig::Load()
{
    // Populate the struct from eeprom
    m_eeprom->Get(0, m_config);

    // Check if version number matches
    if (m_config.version != (uint32_t)(RX_CONFIG_VERSION | RX_CONFIG_MAGIC))
    {
        // If not, revert to defaults for this version
        DBGLN("EEPROM version mismatch! Resetting to defaults...");
        SetDefaults();
    }

    m_modified = false;
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
RxConfig::SetDefaults()
{
    m_config.version = RX_CONFIG_VERSION | RX_CONFIG_MAGIC;
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
    SetSSID("");
    SetPassword("");
#if defined(GPIO_PIN_PWM_OUTPUTS)
    for (unsigned int ch=0; ch<PWM_MAX_CHANNELS; ++ch)
        SetPwmChannel(ch, 512, ch, false);
    SetPwmChannel(2, 0, 2, false); // ch2 is throttle, failsafe it to 988
#endif
    SetOnLoan(false);
    Commit();
}

void
RxConfig::SetStorageProvider(ELRS_EEPROM *eeprom)
{
    if (eeprom)
    {
        m_eeprom = eeprom;
    }
}

void
RxConfig::SetSSID(const char *ssid)
{
    strncpy(m_config.ssid, ssid, sizeof(m_config.ssid)-1);
    m_modified = true;
}

void
RxConfig::SetPassword(const char *password)
{
    strncpy(m_config.password, password, sizeof(m_config.password)-1);
    m_modified = true;
}

#if defined(GPIO_PIN_PWM_OUTPUTS)
void
RxConfig::SetPwmChannel(uint8_t ch, uint16_t failsafe, uint8_t inputCh, bool inverted)
{
    if (ch > PWM_MAX_CHANNELS)
        return;

    rx_config_pwm_t *pwm = &m_config.pwmChannels[ch];
    if (pwm->val.failsafe == failsafe && pwm->val.inputChannel == inputCh
        && pwm->val.inverted == inverted)
        return;

    pwm->val.failsafe = failsafe;
    pwm->val.inputChannel = inputCh;
    pwm->val.inverted = inverted;
    m_modified = true;
}

void
RxConfig::SetPwmChannelRaw(uint8_t ch, uint16_t raw)
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
