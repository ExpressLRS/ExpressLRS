#include "common.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)

#include "SX127xDriver.h"
extern SX127xDriver Radio;

expresslrs_mod_settings_s ExpressLRS_AirRateConfig[RATE_MAX] = {
    {0, RATE_200HZ, SX127x_BW_500_00_KHZ, SX127x_SF_6, SX127x_CR_4_7, 5000, TLM_RATIO_1_64, 4, 8},
    {1, RATE_100HZ, SX127x_BW_500_00_KHZ, SX127x_SF_7, SX127x_CR_4_7, 10000, TLM_RATIO_1_64, 4, 8},
    {2, RATE_50HZ, SX127x_BW_500_00_KHZ, SX127x_SF_8, SX127x_CR_4_7, 20000, TLM_RATIO_NO_TLM, 4, 10},
    {3, RATE_25HZ, SX127x_BW_500_00_KHZ, SX127x_SF_9, SX127x_CR_4_7, 40000, TLM_RATIO_NO_TLM, 4, 10}};

expresslrs_rf_pref_params_s ExpressLRS_AirRateRFperf[RATE_MAX] = {
    {0, RATE_200HZ, -112, 4380, 3500, 2500, 2000, 5000},
    {1, RATE_100HZ, -117, 8770, 3500, 2500, 2000, 5000},
    {2, RATE_50HZ, -120, 17540, 5000, 2500, 2000, 5000},
    {3, RATE_25HZ, -123, 17540, 5000, 4000, 2000, 5000}};
#endif

#if defined(Regulatory_Domain_ISM_2400)

#include "SX1280Driver.h"
extern SX1280Driver Radio;

#ifdef USE_500HZ
expresslrs_mod_settings_s ExpressLRS_AirRateConfig[RATE_MAX] = {
    {0, RATE_500HZ, SX1280_LORA_BW_0800, SX1280_LORA_SF5, SX1280_LORA_CR_LI_4_6, 2000, TLM_RATIO_1_128, 4, 12},
    {1, RATE_250HZ, SX1280_LORA_BW_0800, SX1280_LORA_SF6, SX1280_LORA_CR_LI_4_7, 4000, TLM_RATIO_1_64, 4, 14},
    {2, RATE_150HZ, SX1280_LORA_BW_0800, SX1280_LORA_SF7, SX1280_LORA_CR_LI_4_7, 6666, TLM_RATIO_1_32, 4, 12},
    {3, RATE_50HZ, SX1280_LORA_BW_0800, SX1280_LORA_SF9, SX1280_LORA_CR_LI_4_6, 20000, TLM_RATIO_NO_TLM, 4, 12}};

expresslrs_rf_pref_params_s ExpressLRS_AirRateRFperf[RATE_MAX] = {
    {0, RATE_500HZ, -105, 4380, 3500, 1000, 2000, 5000},
    {1, RATE_250HZ, -108, 4380, 3500, 2500, 2000, 5000},
    {2, RATE_150HZ, -112, 8770, 3500, 2500, 2000, 5000},
    {3, RATE_50HZ, -117, 17540, 5000, 2500, 2000, 5000}};
#else
expresslrs_mod_settings_s ExpressLRS_AirRateConfig[RATE_MAX] = {
    {0, RATE_250HZ, SX1280_LORA_BW_0800, SX1280_LORA_SF6, SX1280_LORA_CR_LI_4_7, 4000, TLM_RATIO_1_64, 4, 14},
    {1, RATE_150HZ, SX1280_LORA_BW_0800, SX1280_LORA_SF7, SX1280_LORA_CR_LI_4_7, 6666, TLM_RATIO_1_32, 4, 12},
    {2, RATE_50HZ, SX1280_LORA_BW_0800, SX1280_LORA_SF9, SX1280_LORA_CR_LI_4_6, 20000, TLM_RATIO_NO_TLM, 4, 12},
    {3, RATE_25HZ, SX1280_LORA_BW_0800, SX1280_LORA_SF10, SX1280_LORA_CR_LI_4_6, 40000, TLM_RATIO_NO_TLM, 4, 12}};

expresslrs_rf_pref_params_s ExpressLRS_AirRateRFperf[RATE_MAX] = {
    {0, RATE_250HZ, -108, 4380, 3500, 2500, 2000, 5000},
    {1, RATE_150HZ, -112, 8770, 3500, 2500, 2000, 5000},
    {2, RATE_50HZ, -117, 17540, 5000, 2500, 2000, 5000},
    {3, RATE_25HZ, -120, 36886, 5000, 4000, 2000, 5000}};
#endif

#endif

expresslrs_mod_settings_s *get_elrs_airRateConfig(int8_t index);
//const expresslrs_mod_settings_s * ExpressLRS_nextAirRate;
expresslrs_mod_settings_s *ExpressLRS_currAirRate;
expresslrs_mod_settings_s *ExpressLRS_prevAirRate;

expresslrs_mod_settings_s *get_elrs_airRateConfig(int8_t index)
{
    // Protect against out of bounds rate
    if (index < 0)
    {
        // Set to first entry in the array
        return &ExpressLRS_AirRateConfig[0];
    }
    else if (index > (RATE_MAX - 1))
    {
        // Set to last usable entry in the array
        return &ExpressLRS_AirRateConfig[RATE_MAX - 1];
    }
    return &ExpressLRS_AirRateConfig[index];
}

expresslrs_rf_pref_params_s *get_elrs_RFperfParams(int8_t index)
{
    // Protect against out of bounds rate
    if (index < 0)
    {
        // Set to first entry in the array
        return &ExpressLRS_AirRateRFperf[0];
    }
    else if (index > (RATE_MAX - 1))
    {
        // Set to last usable entry in the array
        return &ExpressLRS_AirRateRFperf[RATE_MAX - 1];
    }
    return &ExpressLRS_AirRateRFperf[index];
}

ICACHE_RAM_ATTR uint8_t enumRatetoIndex(expresslrs_RFrates_e rate)
{ // convert enum_rate to index
    for (int i = 0; i < RATE_MAX; i++)
    {
        expresslrs_mod_settings_s *const ModParams = get_elrs_airRateConfig(i);
        if (ModParams->enum_rate == rate)
        {
            return i;
        }
    }
    // If 25Hz selected and not available, return the slowest rate available
    // else return the fastest rate available (500Hz selected but not available)
    return (rate == RATE_25HZ) ? RATE_MAX - 1 : 0;
}

expresslrs_mod_settings_s *ExpressLRS_currAirRate_Modparams;
expresslrs_rf_pref_params_s *ExpressLRS_currAirRate_RFperfParams;

uint8_t ExpressLRS_nextAirRateIndex = 0;

connectionState_e connectionState = disconnected;
connectionState_e connectionStatePrev = disconnected;

uint8_t BindingUID[6] = {0, 1, 2, 3, 4, 5}; // Special binding UID values
#if defined(MY_UID)
    uint8_t UID[6] = {MY_UID};
#else
    #ifdef PLATFORM_ESP32
        uint8_t UID[6];
        esp_err_t WiFiErr = esp_read_mac(UID, ESP_MAC_WIFI_STA);
    #elif PLATFORM_STM32
        uint8_t UID[6] = {
            (uint8_t)HAL_GetUIDw0(), (uint8_t)(HAL_GetUIDw0() >> 8),
            (uint8_t)HAL_GetUIDw1(), (uint8_t)(HAL_GetUIDw1() >> 8),
            (uint8_t)HAL_GetUIDw2(), (uint8_t)(HAL_GetUIDw2() >> 8)};
    #else
        uint8_t UID[6] = {0};
    #endif
#endif
uint8_t MasterUID[6] = {UID[0], UID[1], UID[2], UID[3], UID[4], UID[5]}; // Special binding UID values

uint16_t CRCInitializer = (UID[4] << 8) | UID[5];

#define RSSI_FLOOR_NUM_READS 5 // number of times to sweep the noise foor to get avg. RSSI reading
#define MEDIAN_SIZE 20

uint8_t ICACHE_RAM_ATTR TLMratioEnumToValue(expresslrs_tlm_ratio_e enumval)
{
    switch (enumval)
    {
    case TLM_RATIO_NO_TLM:
        return 1;
        break;
    case TLM_RATIO_1_2:
        return 2;
        break;
    case TLM_RATIO_1_4:
        return 4;
        break;
    case TLM_RATIO_1_8:
        return 8;
        break;
    case TLM_RATIO_1_16:
        return 16;
        break;
    case TLM_RATIO_1_32:
        return 32;
        break;
    case TLM_RATIO_1_64:
        return 64;
        break;
    case TLM_RATIO_1_128:
        return 128;
        break;
    default:
        return 0;
    }
}

uint16_t RateEnumToHz(expresslrs_RFrates_e eRate)
{
    switch(eRate)
    {
    case RATE_500HZ: return 500;
    case RATE_250HZ: return 250;
    case RATE_200HZ: return 200;
    case RATE_150HZ: return 150;
    case RATE_100HZ: return 100;
    case RATE_50HZ: return 50;
    case RATE_25HZ: return 25;
    case RATE_4HZ: return 4;
    default: return 1;
    }
}