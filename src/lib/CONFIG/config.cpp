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
        if(!UpgradeEepromV1ToV4())
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
    UpgradeEepromV1ToV4();

    m_eeprom->Get(0, m_config);
    m_modified = 0;

    // Check if version number matches
    if (m_config.version != (uint32_t)(TX_CONFIG_VERSION | TX_CONFIG_MAGIC))
    {
        // If not, revert to defaults for this version
        DBGLN("EEPROM version mismatch! Resetting to defaults...");
        SetDefaults();
        Commit();
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
    Commit();
    for (int i=0 ; i<64 ; i++) {
        SetModelId(i);
        SetRate(modParams->index);
        SetTlm(modParams->TLMinterval);
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
TxConfig::UpgradeEepromV1ToV4()
{
    // Define a config struct based on the v1 EEPROM schema
    struct Version1Config {
        uint32_t    version;
        uint32_t    rate;
        uint32_t    tlm;
        uint32_t    power;
    };

    Version1Config v1Config;

    // Populate the v1 struct from eeprom
    m_eeprom->Get(0, v1Config);
    if (v1Config.version != 1)
    {
        return false;
    }

    DBGLN("EEPROM version 1 is out of date... upgrading to version %u", TX_CONFIG_VERSION);

    v1Config.version = 0;
    m_eeprom->Put(0, v1Config);
    m_eeprom->Commit();

    // Set new config to old values, or defaults if they are new variables
    m_config.version = TX_CONFIG_VERSION | TX_CONFIG_MAGIC;

    SetSSID("");
    SetPassword("");

    SetVtxBand(0);
    SetVtxChannel(0);
    SetVtxPower(0);
    SetVtxPitmode(0);
    SetPowerFanThreshold(PWR_250mW);
    Commit();

    for (int i = 0; i < 64; ++i)
    {
        SetModelId(i);
        SetRate(v1Config.rate);
        SetTlm(v1Config.tlm);
        SetPower(v1Config.power);
        SetDynamicPower(0);
        SetBoostChannel(0);
        SetSwitchMode((uint8_t)smHybrid);
        SetModelMatch(false);
        SetFanMode(0);
        SetMotionMode(0);
        Commit();
    }

    SetModelId(0);
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
#if defined(GPIO_PIN_ANTENNA_SELECT) && defined(USE_DIVERSITY)
    SetAntennaMode(2); //2 is diversity
#else
    SetAntennaMode(1); //0 and 1 is use for gpio_antenna_select
#endif
    SetSSID("");
    SetPassword("");
#if defined(GPIO_PIN_PWM_OUTPUTS)
    for (unsigned int ch=0; ch<PWM_MAX_CHANNELS; ++ch)
        SetPwmChannel(ch, 512, ch, false);
    SetPwmChannel(2, 0, 2, false); // ch2 is throttle, failsafe it to 988
#endif
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
