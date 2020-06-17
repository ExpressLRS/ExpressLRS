#include "common.h"

extern SX127xDriver Radio;

expresslrs_mod_settings_s ExpressLRS_AirRateConfig[RATE_MAX] = {
    {RATE_200HZ, SX127x_BW_500_00_KHZ, SX127x_SF_6, SX127x_CR_4_7, 5000, 200, TLM_RATIO_1_64, 2, 8},
    {RATE_100HZ, SX127x_BW_500_00_KHZ, SX127x_SF_7, SX127x_CR_4_7, 10000, 100, TLM_RATIO_1_64, 2, 8},
    {RATE_50HZ, SX127x_BW_500_00_KHZ, SX127x_SF_8, SX127x_CR_4_7, 20000, 50, TLM_RATIO_NO_TLM, 2, 8},
    {RATE_25HZ, SX127x_BW_500_00_KHZ, SX127x_SF_9, SX127x_CR_4_7, 40000, 25, TLM_RATIO_NO_TLM, 2, 10},
    {RATE_4HZ, SX127x_BW_500_00_KHZ, SX127x_SF_12, SX127x_CR_4_7, 250000, 4, TLM_RATIO_1_4, 2, 10}}; // for model recovery 

expresslrs_rf_pref_params_s ExpressLRS_AirRateRFperf[RATE_MAX] = {
    {RATE_200HZ, -112, 4380, 2500, 1000, 2000, 5000}, // ~ 3 sync packets
    {RATE_100HZ, -117, 8770, 2500, 2000, 2000, 5000},
    {RATE_50HZ, -120, 17540, 2500, 4000, 2000, 5000},
    {RATE_25HZ, -123, 17540, 5000, 8000, 2000, 5000},
    {RATE_4HZ, -131, 239620, 30000, 60000, 0, 250}}; // this means always send sync on ch[0] as soon as we can 

ICACHE_RAM_ATTR expresslrs_mod_settings_s *get_elrs_airRateConfig(expresslrs_RFrates_e rate)
{
    // Protect against out of bounds rate
    if (rate < 0)
    {
        // Set to first entry in the array (200HZ)
        return &ExpressLRS_AirRateConfig[0];
    }
    else if (rate > (RATE_MAX - 1))
    {
        // Set to last usable entry in the array (currently 50HZ)
        return &ExpressLRS_AirRateConfig[RATE_MAX - 1];
    }
    return &ExpressLRS_AirRateConfig[rate];
}

ICACHE_RAM_ATTR expresslrs_rf_pref_params_s *get_elrs_RFperfParams(expresslrs_RFrates_e rate)
{
    // Protect against out of bounds rate
    if (rate < 0)
    {
        // Set to first entry in the array (200HZ)
        return &ExpressLRS_AirRateRFperf[0];
    }
    else if (rate > (RATE_MAX - 1))
    {
        // Set to last usable entry in the array (currently 50HZ)
        return &ExpressLRS_AirRateRFperf[RATE_MAX - 1];
    }
    return &ExpressLRS_AirRateRFperf[rate];
}

expresslrs_mod_settings_s *ExpressLRS_currAirRate_Modparams;
expresslrs_rf_pref_params_s *ExpressLRS_currAirRate_RFperfParams;

//expresslrs_mod_settings_s *ExpressLRS_nextAirRate;
//expresslrs_mod_settings_s *ExpressLRS_prevAirRate;
bool ExpressLRS_AirRateNeedsUpdate = false;

connectionState_e connectionState = disconnected;
connectionState_e connectionStatePrev = disconnected;

uint32_t HashedBindingPhrase = HASH(My_Binding_Phrase);
uint8_t UID[6] = {0,
                  0,
                  HashedBindingPhrase & 0b11111111,
                  (HashedBindingPhrase >> 8) & 0b11111111,
                  (HashedBindingPhrase >> 16) & 0b11111111,
                  (HashedBindingPhrase >> 24) & 0b11111111
                 };

uint8_t CRCCaesarCipher = UID[4];
uint8_t DeviceAddr = UID[5] & 0b111111; // temporarily based on mac until listen before assigning method merged

#define RSSI_FLOOR_NUM_READS 5 // number of times to sweep the noise foor to get avg. RSSI reading
#define MEDIAN_SIZE 20

uint8_t ICACHE_RAM_ATTR TLMratioEnumToValue(expresslrs_tlm_ratio_e enumval)
{
    switch (enumval)
    {
    case TLM_RATIO_NO_TLM:
        return 0;
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
