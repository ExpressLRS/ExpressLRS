#include "common.h"
#include "OTA.h"

// Sanity checks
static_assert(RATE_DEFAULT < RATE_MAX, "Default rate must be below RATE_MAX");
static_assert(RATE_BINDING < RATE_MAX, "Binding rate must be below RATE_MAX");


#if defined(RADIO_SX127X)

#include "SX127xDriver.h"
SX127xDriver DMA_ATTR Radio;

expresslrs_mod_settings_s ExpressLRS_AirRateConfig[RATE_MAX] = {
    {0, RADIO_TYPE_SX127x_LORA, RATE_LORA_200HZ,     SX127x_BW_500_00_KHZ, SX127x_SF_6, SX127x_CR_4_7, TLM_RATIO_1_64,   4,  5000,  8, OTA4_PACKET_SIZE, 1},
    {1, RADIO_TYPE_SX127x_LORA, RATE_LORA_100HZ_8CH, SX127x_BW_500_00_KHZ, SX127x_SF_6, SX127x_CR_4_8, TLM_RATIO_1_32,   4, 10000,  8, OTA8_PACKET_SIZE, 1},
    {2, RADIO_TYPE_SX127x_LORA, RATE_LORA_100HZ,     SX127x_BW_500_00_KHZ, SX127x_SF_7, SX127x_CR_4_7, TLM_RATIO_1_32,   4, 10000,  8, OTA4_PACKET_SIZE, 1},
    {3, RADIO_TYPE_SX127x_LORA, RATE_LORA_50HZ,      SX127x_BW_500_00_KHZ, SX127x_SF_8, SX127x_CR_4_7, TLM_RATIO_NO_TLM, 4, 20000, 10, OTA4_PACKET_SIZE, 1},
    {4, RADIO_TYPE_SX127x_LORA, RATE_LORA_25HZ,      SX127x_BW_500_00_KHZ, SX127x_SF_9, SX127x_CR_4_7, TLM_RATIO_NO_TLM, 2, 40000, 10, OTA4_PACKET_SIZE, 1}};

expresslrs_rf_pref_params_s ExpressLRS_AirRateRFperf[RATE_MAX] = {
    {0, RATE_LORA_200HZ,     -112,  4380, 3000, 2500, 600, 5000},
    {1, RATE_LORA_100HZ_8CH, -112,  6690, 3500, 2500, 600, 5000},
    {2, RATE_LORA_100HZ,     -117,  8770, 3500, 2500, 600, 5000},
    {3, RATE_LORA_50HZ,      -120, 18560, 4000, 2500, 600, 5000},
    {4, RATE_LORA_25HZ,      -123, 29950, 6000, 4000,   0, 5000}};
#endif

#if defined(RADIO_SX128X)

#include "SX1280Driver.h"
SX1280Driver DMA_ATTR Radio;

expresslrs_mod_settings_s ExpressLRS_AirRateConfig[RATE_MAX] = {
    {0, RADIO_TYPE_SX128x_FLRC, RATE_FLRC_1000HZ,    SX1280_FLRC_BR_0_650_BW_0_6, SX1280_FLRC_BT_1, SX1280_FLRC_CR_1_2,    TLM_RATIO_1_128,  2,  1000, 32, OTA4_PACKET_SIZE, 1},
    {1, RADIO_TYPE_SX128x_FLRC, RATE_FLRC_500HZ,     SX1280_FLRC_BR_0_650_BW_0_6, SX1280_FLRC_BT_1, SX1280_FLRC_CR_1_2,    TLM_RATIO_1_128,  2,  2000, 32, OTA4_PACKET_SIZE, 1},
    {2, RADIO_TYPE_SX128x_FLRC, RATE_DVDA_500HZ,     SX1280_FLRC_BR_0_650_BW_0_6, SX1280_FLRC_BT_1, SX1280_FLRC_CR_1_2,    TLM_RATIO_1_128,  2,  1000, 32, OTA4_PACKET_SIZE, 2},
    {3, RADIO_TYPE_SX128x_FLRC, RATE_DVDA_250HZ,     SX1280_FLRC_BR_0_650_BW_0_6, SX1280_FLRC_BT_1, SX1280_FLRC_CR_1_2,    TLM_RATIO_1_128,  2,  1000, 32, OTA4_PACKET_SIZE, 4},
    {4, RADIO_TYPE_SX128x_LORA, RATE_LORA_500HZ,     SX1280_LORA_BW_0800,         SX1280_LORA_SF5,  SX1280_LORA_CR_LI_4_6, TLM_RATIO_1_128,  4,  2000, 12, OTA4_PACKET_SIZE, 1},
    {5, RADIO_TYPE_SX128x_LORA, RATE_LORA_333HZ_8CH, SX1280_LORA_BW_0800,         SX1280_LORA_SF5,  SX1280_LORA_CR_LI_4_7, TLM_RATIO_1_128,  4,  3003, 12, OTA8_PACKET_SIZE, 1},
    {6, RADIO_TYPE_SX128x_LORA, RATE_LORA_250HZ,     SX1280_LORA_BW_0800,         SX1280_LORA_SF6,  SX1280_LORA_CR_LI_4_7, TLM_RATIO_1_64,   4,  4000, 14, OTA4_PACKET_SIZE, 1},
    {7, RADIO_TYPE_SX128x_LORA, RATE_LORA_150HZ,     SX1280_LORA_BW_0800,         SX1280_LORA_SF7,  SX1280_LORA_CR_LI_4_7, TLM_RATIO_1_32,   4,  6666, 12, OTA4_PACKET_SIZE, 1},
    {8, RADIO_TYPE_SX128x_LORA, RATE_LORA_100HZ_8CH, SX1280_LORA_BW_0800,         SX1280_LORA_SF7,  SX1280_LORA_CR_LI_4_7, TLM_RATIO_1_32,   4, 10000, 12, OTA8_PACKET_SIZE, 1},
    {9, RADIO_TYPE_SX128x_LORA, RATE_LORA_50HZ,      SX1280_LORA_BW_0800,         SX1280_LORA_SF9,  SX1280_LORA_CR_LI_4_6, TLM_RATIO_NO_TLM, 2, 20000, 12, OTA4_PACKET_SIZE, 1}};

expresslrs_rf_pref_params_s ExpressLRS_AirRateRFperf[RATE_MAX] = {
    {0, RATE_FLRC_1000HZ,    -104,   389, 2500, 2500,  3, 5000},
    {1, RATE_FLRC_500HZ,     -104,   389, 2500, 2500,  3, 5000},
    {2, RATE_DVDA_500HZ,     -104,   389, 2500, 2500,  3, 5000},
    {3, RATE_DVDA_250HZ,     -104,   389, 2500, 2500,  3, 5000},
    {4, RATE_LORA_500HZ,     -105,  1507, 2500, 2500,  3, 5000},
    {5, RATE_LORA_333HZ_8CH, -105,  2374, 2500, 2500,  4, 5000},
    {6, RATE_LORA_250HZ,     -108,  3300, 3000, 2500,  6, 5000},
    {7, RATE_LORA_150HZ,     -112,  5871, 3500, 2500, 10, 5000},
    {8, RATE_LORA_100HZ_8CH, -112,  7605, 3500, 2500, 11, 5000},
    {9, RATE_LORA_50HZ,      -117, 18443, 4000, 2500,  0, 5000}};
#endif

expresslrs_mod_settings_s *get_elrs_airRateConfig(uint8_t index)
{
    if (RATE_MAX <= index)
    {
        // Set to last usable entry in the array
        index = RATE_MAX - 1;
    }
    return &ExpressLRS_AirRateConfig[index];
}

expresslrs_rf_pref_params_s *get_elrs_RFperfParams(uint8_t index)
{
    if (RATE_MAX <= index)
    {
        // Set to last usable entry in the array
        index = RATE_MAX - 1;
    }
    return &ExpressLRS_AirRateRFperf[index];
}

uint8_t ICACHE_RAM_ATTR enumRatetoIndex(expresslrs_RFrates_e const eRate)
{ // convert enum_rate to index
    expresslrs_mod_settings_s const * ModParams;
    for (uint8_t i = 0; i < RATE_MAX; i++)
    {
        ModParams = get_elrs_airRateConfig(i);
        if (ModParams->enum_rate == eRate)
        {
            return i;
        }
    }
    // If 25Hz selected and not available, return the slowest rate available
    // else return the fastest rate available (500Hz selected but not available)
    return (eRate == RATE_LORA_25HZ) ? RATE_MAX - 1 : 0;
}

expresslrs_mod_settings_s *ExpressLRS_currAirRate_Modparams;
expresslrs_rf_pref_params_s *ExpressLRS_currAirRate_RFperfParams;

connectionState_e connectionState = disconnected;
bool connectionHasModelMatch;

uint8_t MasterUID[6];                       // The definitive user UID
uint8_t UID[6];                             // The currently running UID
uint8_t BindingUID[6] = {0, 1, 2, 3, 4, 5}; // Special binding UID values

uint8_t ICACHE_RAM_ATTR TLMratioEnumToValue(uint8_t const enumval)
{
    switch (enumval)
    {
    case TLM_RATIO_NO_TLM:
        return 1;
    case TLM_RATIO_1_2:
        return 2;
    case TLM_RATIO_1_4:
        return 4;
    case TLM_RATIO_1_8:
        return 8;
    case TLM_RATIO_1_16:
        return 16;
    case TLM_RATIO_1_32:
        return 32;
    case TLM_RATIO_1_64:
        return 64;
    case TLM_RATIO_1_128:
        return 128;
    default:
        return 0;
    }
}

/***
 * @brief: Calculate number of 'burst' telemetry frames for the specified air rate and tlm ratio
 *
 * When attempting to send a LinkStats telemetry frame at most every TELEM_MIN_LINK_INTERVAL_MS,
 * calculate the number of sequential advanced telemetry frames before another LinkStats is due.
 ****/
uint8_t TLMBurstMaxForRateRatio(uint16_t const rateHz, uint8_t const ratioDiv)
{
    // Maximum ms between LINK_STATISTICS packets for determining burst max
    constexpr uint32_t TELEM_MIN_LINK_INTERVAL_MS = 512U;

    // telemInterval = 1000 / (hz / ratiodiv);
    // burst = TELEM_MIN_LINK_INTERVAL_MS / telemInterval;
    // This ^^^ rearranged to preserve precision vvv
    uint8_t retVal = TELEM_MIN_LINK_INTERVAL_MS * rateHz / ratioDiv / 1000U;

    // Reserve one slot for LINK telemetry
    if (retVal > 1)
        --retVal;
    else
        retVal = 1;
    //DBGLN("TLMburst: %d", retVal);

    return retVal;
}


uint16_t RateEnumToHz(expresslrs_RFrates_e const eRate)
{
    switch(eRate)
    {
    case RATE_FLRC_1000HZ: return 1000;
    case RATE_FLRC_500HZ: return 500;
    case RATE_DVDA_500HZ: return 500;
    case RATE_DVDA_250HZ: return 250;
    case RATE_LORA_500HZ: return 500;
    case RATE_LORA_333HZ_8CH: return 333;
    case RATE_LORA_250HZ: return 250;
    case RATE_LORA_200HZ: return 200;
    case RATE_LORA_150HZ: return 150;
    case RATE_LORA_100HZ: return 100;
    case RATE_LORA_100HZ_8CH: return 100;
    case RATE_LORA_50HZ: return 50;
    case RATE_LORA_25HZ: return 25;
    case RATE_LORA_4HZ: return 4;
    default: return 1;
    }
}

uint32_t uidMacSeedGet(void)
{
    const uint32_t macSeed = ((uint32_t)UID[2] << 24) + ((uint32_t)UID[3] << 16) +
                             ((uint32_t)UID[4] << 8) + UID[5];
    return macSeed;
}

void initUID()
{
    if (firmwareOptions.hasUID)
    {
        memcpy(MasterUID, firmwareOptions.uid, sizeof(MasterUID));
    }
    else
    {
    #ifdef PLATFORM_ESP32
        esp_err_t WiFiErr = esp_read_mac(MasterUID, ESP_MAC_WIFI_STA);
    #elif PLATFORM_STM32
        MasterUID[0] = (uint8_t)HAL_GetUIDw0();
        MasterUID[1] = (uint8_t)(HAL_GetUIDw0() >> 8);
        MasterUID[2] = (uint8_t)HAL_GetUIDw1();
        MasterUID[3] = (uint8_t)(HAL_GetUIDw1() >> 8);
        MasterUID[4] = (uint8_t)HAL_GetUIDw2();
        MasterUID[5] = (uint8_t)(HAL_GetUIDw2() >> 8);
    #endif
    }
    memcpy(UID, MasterUID, sizeof(UID));
    OtaUpdateCrcInitFromUid();
}