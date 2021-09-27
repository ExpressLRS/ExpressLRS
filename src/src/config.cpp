#include "config.h"
#include "common.h"
#include "POWERMGNT.h"
#include "OTA.h"
#include "logging.h"

#if defined(TARGET_TX)

#define MODEL_CHANGED       0x01
#define VTX_CHANGED         0x02
#define SSID_CHANGED        0x04
#define PASSWORD_CHANGED    0x08

TxConfig::TxConfig()
{
    SetModelId(0);
    m_eeprom.Begin();
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
    if(nvs_get_u32(handle, "tx_version", &m_config.version) != ESP_ERR_NVS_NOT_FOUND)
    {
        uint32_t value;
        nvs_get_u32(handle, "vtx", &value);
        m_config.vtxBand = value >> 24;
        m_config.vtxChannel = value >> 16;
        m_config.vtxPower = value >> 8;
        m_config.vtxPitmode = value;
        value = sizeof(m_config.ssid);
        nvs_get_str(handle, "ssid", m_config.ssid, &value);
        value = sizeof(m_config.password);
        nvs_get_str(handle, "password", m_config.password, &value);
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
        // Read old eeprom config
        m_eeprom.Get(0, m_config);

        // Check if version number matches
        if (m_config.version != (uint32_t)(TX_CONFIG_VERSION | TX_CONFIG_MAGIC))
        {
            // If the previous config schema is known, attempt an upgrade
            if (m_config.version == 1)
            {
                UpgradeEepromV1ToV4();
            }
            else
            {
                // If not, revert to defaults for this version
                DBGLN("EEPROM version mismatch! Resetting to defaults...");
                SetDefaults();
            }
        }

        nvs_set_u32(handle, "tx_version", TX_CONFIG_VERSION | TX_CONFIG_MAGIC);
        Commit();
    }
    m_model = &m_config.model_config[m_modelId];
    m_modified = 0;
}
#else
void
TxConfig::Load()
{
    m_eeprom.Get(0, m_config);

    m_modified = 0;
    // Check if version number matches
    if (m_config.version != (uint32_t)(TX_CONFIG_VERSION | TX_CONFIG_MAGIC))
    {
        // If the previous config schema is known, attempt an upgrade
        if (m_config.version == 1)
        {
            UpgradeEepromV1ToV4();
        }
        else
        {
            // If not, revert to defaults for this version
            DBGLN("EEPROM version mismatch! Resetting to defaults...");
            SetDefaults();
        }
    }
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
    if (m_modified & SSID_CHANGED)
        nvs_set_str(handle, "ssid", m_config.ssid);
    if (m_modified & PASSWORD_CHANGED)
        nvs_set_str(handle, "password", m_config.password);
    nvs_commit(handle);
#else
    // Write the struct to eeprom
    m_eeprom.Put(0, m_config);
    m_eeprom.Commit();
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
TxConfig::SetDefaults()
{
    expresslrs_mod_settings_s *const modParams = get_elrs_airRateConfig(RATE_DEFAULT);
    m_config.version = TX_CONFIG_VERSION | TX_CONFIG_MAGIC;
    SetSSID("");
    SetPassword("");
    for (int i=0 ; i<64 ; i++) {
        SetModelId(i);
        SetRate(modParams->index);
        SetTlm(modParams->TLMinterval);
        SetPower(DefaultPowerEnum);
        SetDynamicPower(0);
        SetBoostChannel(0);
        SetSwitchMode((uint8_t)smHybrid);
        SetModelMatch(false);
    }
    SetVtxBand(0);
    SetVtxChannel(0);
    SetVtxPower(0);
    SetVtxPitmode(0);
    Commit();

    SetModelId(0);
}

void
TxConfig::UpgradeEepromV1ToV4()
{
    DBGLN("EEPROM version 1 is out of date... upgrading to version 4");
    
    // Define a config struct based on the v1 EEPROM schema
    struct Version1Config {
        uint32_t    version;
        uint32_t    rate;
        uint32_t    tlm;
        uint32_t    power;
    };

    Version1Config v1Config;

    // Populate the v1 struct from eeprom
    m_eeprom.Get(0, v1Config);
    
    // Set new config to old values, or defaults if they are new variables
    m_config.version = TX_CONFIG_VERSION | TX_CONFIG_MAGIC;

    SetSSID("");
    SetPassword("");

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
    }

    SetVtxBand(0);
    SetVtxChannel(0);
    SetVtxPower(0);
    SetVtxPitmode(0);
    Commit();

    SetModelId(0);
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
RxConfig::SetDefaults()
{
    m_config.version = RX_CONFIG_VERSION | RX_CONFIG_MAGIC;
    SetIsBound(false);
    SetPowerOnCounter(0);
    SetModelId(0xFF);
    SetSSID("");
    SetPassword("");
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

#endif