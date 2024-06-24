#include "common.h"
#include "OTA.h"

#if defined(RADIO_SX127X)

#include "SX127xDriver.h"
SX127xDriver DMA_ATTR Radio;

expresslrs_mod_settings_s ExpressLRS_AirRateConfig[RATE_MAX] = {
    {0, RADIO_TYPE_SX127x_LORA, RATE_LORA_200HZ,     SX127x_BW_500_00_KHZ, SX127x_SF_6, SX127x_CR_4_7,  8, TLM_RATIO_1_64, 4,  5000, OTA4_PACKET_SIZE, 1},
    {1, RADIO_TYPE_SX127x_LORA, RATE_LORA_100HZ_8CH, SX127x_BW_500_00_KHZ, SX127x_SF_6, SX127x_CR_4_8,  8, TLM_RATIO_1_32, 4, 10000, OTA8_PACKET_SIZE, 1},
    {2, RADIO_TYPE_SX127x_LORA, RATE_LORA_100HZ,     SX127x_BW_500_00_KHZ, SX127x_SF_7, SX127x_CR_4_7,  8, TLM_RATIO_1_32, 4, 10000, OTA4_PACKET_SIZE, 1},
    {3, RADIO_TYPE_SX127x_LORA, RATE_LORA_50HZ,      SX127x_BW_500_00_KHZ, SX127x_SF_8, SX127x_CR_4_7, 10, TLM_RATIO_1_16, 4, 20000, OTA4_PACKET_SIZE, 1},
    {4, RADIO_TYPE_SX127x_LORA, RATE_LORA_25HZ,      SX127x_BW_500_00_KHZ, SX127x_SF_9, SX127x_CR_4_7, 10, TLM_RATIO_1_8,  2, 40000, OTA4_PACKET_SIZE, 1},
    {5, RADIO_TYPE_SX127x_LORA, RATE_DVDA_50HZ,      SX127x_BW_500_00_KHZ, SX127x_SF_6, SX127x_CR_4_7,  8, TLM_RATIO_1_64, 2,  5000, OTA4_PACKET_SIZE, 4}};

expresslrs_rf_pref_params_s ExpressLRS_AirRateRFperf[RATE_MAX] = {
    {0, RATE_LORA_200HZ,     -112,  4380, 3000, 2500, 600, 5000, SNR_SCALE( 1), SNR_SCALE(3.0)},
    {1, RATE_LORA_100HZ_8CH, -112,  6690, 3500, 2500, 600, 5000, SNR_SCALE( 1), SNR_SCALE(3.0)},
    {2, RATE_LORA_100HZ,     -117,  8770, 3500, 2500, 600, 5000, SNR_SCALE( 1), SNR_SCALE(2.5)},
    {3, RATE_LORA_50HZ,      -120, 18560, 4000, 2500, 600, 5000, SNR_SCALE(-1), SNR_SCALE(1.5)},
    {4, RATE_LORA_25HZ,      -123, 29950, 6000, 4000, 600, 5000, SNR_SCALE(-3), SNR_SCALE(0.5)},
    {5, RATE_DVDA_50HZ,      -112,  4380, 3000, 2500, 600, 5000, SNR_SCALE( 1), SNR_SCALE(3.0)}};
#endif

#if defined(RADIO_LR1121)

#include "LR1121Driver.h"
LR1121Driver DMA_ATTR Radio;

expresslrs_mod_settings_s ExpressLRS_AirRateConfig[RATE_MAX] = {
    {0,  RADIO_TYPE_LR1121_LORA_900,  RATE_LORA_200HZ,     LR11XX_RADIO_LORA_BW_500, LR11XX_RADIO_LORA_SF6, LR11XX_RADIO_LORA_CR_4_7,    12, LR11XX_RADIO_LORA_BW_500, LR11XX_RADIO_LORA_SF6, LR11XX_RADIO_LORA_CR_4_7,    12, TLM_RATIO_NO_TLM,4, 10000, OTA4_PACKET_SIZE, 1},
    {1,  RADIO_TYPE_LR1121_LORA_900,  RATE_LORA_100HZ_8CH, LR11XX_RADIO_LORA_BW_500, LR11XX_RADIO_LORA_SF6, LR11XX_RADIO_LORA_CR_4_8,     8, LR11XX_RADIO_LORA_BW_500, LR11XX_RADIO_LORA_SF6, LR11XX_RADIO_LORA_CR_4_8,     8, TLM_RATIO_1_32,  4, 10000, OTA8_PACKET_SIZE, 1},
    {2,  RADIO_TYPE_LR1121_LORA_900,  RATE_LORA_100HZ,     LR11XX_RADIO_LORA_BW_500, LR11XX_RADIO_LORA_SF7, LR11XX_RADIO_LORA_CR_4_7,     8, LR11XX_RADIO_LORA_BW_500, LR11XX_RADIO_LORA_SF7, LR11XX_RADIO_LORA_CR_4_7,     8, TLM_RATIO_1_32,  4, 10000, OTA4_PACKET_SIZE, 1},
    {3,  RADIO_TYPE_LR1121_LORA_900,  RATE_LORA_50HZ,      LR11XX_RADIO_LORA_BW_500, LR11XX_RADIO_LORA_SF8, LR11XX_RADIO_LORA_CR_4_7,    10, LR11XX_RADIO_LORA_BW_500, LR11XX_RADIO_LORA_SF8, LR11XX_RADIO_LORA_CR_4_7,    10, TLM_RATIO_1_16,  4, 20000, OTA4_PACKET_SIZE, 1},
    {4,  RADIO_TYPE_LR1121_LORA_2G4,  RATE_LORA_500HZ,     LR11XX_RADIO_LORA_BW_800, LR11XX_RADIO_LORA_SF5, LR11XX_RADIO_LORA_CR_LI_4_6, 12, LR11XX_RADIO_LORA_BW_800, LR11XX_RADIO_LORA_SF5, LR11XX_RADIO_LORA_CR_LI_4_6, 12, TLM_RATIO_1_128, 4,  2000, OTA4_PACKET_SIZE, 1},
    {5,  RADIO_TYPE_LR1121_LORA_2G4,  RATE_LORA_333HZ_8CH, LR11XX_RADIO_LORA_BW_800, LR11XX_RADIO_LORA_SF5, LR11XX_RADIO_LORA_CR_LI_4_8, 12, LR11XX_RADIO_LORA_BW_800, LR11XX_RADIO_LORA_SF5, LR11XX_RADIO_LORA_CR_LI_4_8, 12, TLM_RATIO_1_128, 4,  3003, OTA8_PACKET_SIZE, 1},
    {6,  RADIO_TYPE_LR1121_LORA_2G4,  RATE_LORA_250HZ,     LR11XX_RADIO_LORA_BW_800, LR11XX_RADIO_LORA_SF6, LR11XX_RADIO_LORA_CR_LI_4_8, 14, LR11XX_RADIO_LORA_BW_800, LR11XX_RADIO_LORA_SF6, LR11XX_RADIO_LORA_CR_LI_4_8, 14, TLM_RATIO_1_64,  4,  4000, OTA4_PACKET_SIZE, 1},
    {7,  RADIO_TYPE_LR1121_LORA_2G4,  RATE_LORA_150HZ,     LR11XX_RADIO_LORA_BW_800, LR11XX_RADIO_LORA_SF7, LR11XX_RADIO_LORA_CR_LI_4_8, 12, LR11XX_RADIO_LORA_BW_800, LR11XX_RADIO_LORA_SF7, LR11XX_RADIO_LORA_CR_LI_4_8, 12, TLM_RATIO_1_32,  4,  6666, OTA4_PACKET_SIZE, 1},
    {8,  RADIO_TYPE_LR1121_LORA_2G4,  RATE_LORA_100HZ_8CH, LR11XX_RADIO_LORA_BW_800, LR11XX_RADIO_LORA_SF7, LR11XX_RADIO_LORA_CR_LI_4_8, 12, LR11XX_RADIO_LORA_BW_800, LR11XX_RADIO_LORA_SF7, LR11XX_RADIO_LORA_CR_LI_4_8, 12, TLM_RATIO_1_32,  4, 10000, OTA8_PACKET_SIZE, 1},
    {9,  RADIO_TYPE_LR1121_LORA_2G4,  RATE_LORA_50HZ,      LR11XX_RADIO_LORA_BW_800, LR11XX_RADIO_LORA_SF8, LR11XX_RADIO_LORA_CR_LI_4_8, 12, LR11XX_RADIO_LORA_BW_800, LR11XX_RADIO_LORA_SF8, LR11XX_RADIO_LORA_CR_LI_4_8, 12, TLM_RATIO_1_16,  2, 20000, OTA4_PACKET_SIZE, 1},
    {10, RADIO_TYPE_LR1121_LORA_DUAL, RATE_LORA_150HZ,     LR11XX_RADIO_LORA_BW_500, LR11XX_RADIO_LORA_SF6, LR11XX_RADIO_LORA_CR_4_8,    12, LR11XX_RADIO_LORA_BW_800, LR11XX_RADIO_LORA_SF7, LR11XX_RADIO_LORA_CR_LI_4_6, 12, TLM_RATIO_1_32,  4,  6666, OTA4_PACKET_SIZE, 1},
    {11, RADIO_TYPE_LR1121_LORA_DUAL, RATE_LORA_100HZ_8CH, LR11XX_RADIO_LORA_BW_500, LR11XX_RADIO_LORA_SF6, LR11XX_RADIO_LORA_CR_4_8,    18, LR11XX_RADIO_LORA_BW_800, LR11XX_RADIO_LORA_SF7, LR11XX_RADIO_LORA_CR_LI_4_8, 14, TLM_RATIO_1_32,  4, 10000, OTA8_PACKET_SIZE, 1},
    {12, RADIO_TYPE_LR1121_LORA_900,  RATE_LORA_250HZ,     LR11XX_RADIO_LORA_BW_500, LR11XX_RADIO_LORA_SF5, LR11XX_RADIO_LORA_CR_4_8,     8, LR11XX_RADIO_LORA_BW_500, LR11XX_RADIO_LORA_SF5, LR11XX_RADIO_LORA_CR_4_8,     8, TLM_RATIO_1_64,  4,  4000, OTA4_PACKET_SIZE, 1},
    {13, RADIO_TYPE_LR1121_LORA_900,  RATE_LORA_200HZ_8CH, LR11XX_RADIO_LORA_BW_500, LR11XX_RADIO_LORA_SF5, LR11XX_RADIO_LORA_CR_4_7,     8, LR11XX_RADIO_LORA_BW_500, LR11XX_RADIO_LORA_SF5, LR11XX_RADIO_LORA_CR_4_7,     8, TLM_RATIO_1_64,  4,  5000, OTA8_PACKET_SIZE, 1}};

expresslrs_rf_pref_params_s ExpressLRS_AirRateRFperf[RATE_MAX] = {
    {0,  RATE_LORA_200HZ,     -112,  4380, 3000, 2500, 600,  5000, SNR_SCALE( 1), SNR_SCALE(3.0)}, // These SNR_SCALE values all need to be checked!
    {1,  RATE_LORA_100HZ_8CH, -112,  6690, 3500, 2500, 600,  5000, SNR_SCALE( 1), SNR_SCALE(3.0)},
    {2,  RATE_LORA_100HZ,     -117,  8770, 3500, 2500, 600,  5000, SNR_SCALE( 1), SNR_SCALE(2.5)},
    {3,  RATE_LORA_50HZ,      -120, 18560, 4000, 2500, 600,  5000, SNR_SCALE(-1), SNR_SCALE(1.5)},
    {4,  RATE_LORA_500HZ,     -105,  1507, 2500, 2500,   3,  5000, SNR_SCALE( 5), SNR_SCALE(9.5)},
    {5,  RATE_LORA_333HZ_8CH, -105,  2374, 2500, 2500,   4,  5000, SNR_SCALE( 5), SNR_SCALE(9.5)},
    {6,  RATE_LORA_250HZ,     -108,  3300, 3000, 2500,   6,  5000, SNR_SCALE( 3), SNR_SCALE(9.5)},
    {7,  RATE_LORA_150HZ,     -112,  5871, 3500, 2500,  10,  5000, SNR_SCALE( 0), SNR_SCALE(8.5)},
    {8,  RATE_LORA_100HZ_8CH, -112,  7605, 3500, 2500,  11,  5000, SNR_SCALE( 0), SNR_SCALE(8.5)},
    {9,  RATE_LORA_50HZ,      -115, 10798, 4000, 2500,   0,  5000, SNR_SCALE(-1), SNR_SCALE(6.5)},
    {10, RATE_LORA_150HZ,     -112,  5871, 3500, 2500,  10,  5000, SNR_SCALE( 0), SNR_SCALE(8.5)},
    {11, RATE_LORA_100HZ_8CH, -112,  7456, 3500, 2500,  11,  5000, SNR_SCALE( 0), SNR_SCALE(8.5)},
    {12, RATE_LORA_250HZ,     -111,  3216, 3500, 2500, 600,  5000, SNR_SCALE( 1), SNR_SCALE(3.0)},
    {13, RATE_LORA_200HZ_8CH, -111,  4240, 3500, 2500, 600,  5000, SNR_SCALE( 1), SNR_SCALE(3.0)}};
#endif

#if defined(RADIO_SX128X)

#include "SX1280Driver.h"
SX1280Driver DMA_ATTR Radio;

expresslrs_mod_settings_s ExpressLRS_AirRateConfig[RATE_MAX] = {
    {0, RADIO_TYPE_SX128x_FLRC, RATE_FLRC_1000HZ,    SX1280_FLRC_BR_0_650_BW_0_6, SX1280_FLRC_BT_1, SX1280_FLRC_CR_1_2,    32, TLM_RATIO_1_128, 2,  1000, OTA4_PACKET_SIZE, 1},
    {1, RADIO_TYPE_SX128x_FLRC, RATE_FLRC_500HZ,     SX1280_FLRC_BR_0_650_BW_0_6, SX1280_FLRC_BT_1, SX1280_FLRC_CR_1_2,    32, TLM_RATIO_1_128, 2,  2000, OTA4_PACKET_SIZE, 1},
    {2, RADIO_TYPE_SX128x_FLRC, RATE_DVDA_500HZ,     SX1280_FLRC_BR_0_650_BW_0_6, SX1280_FLRC_BT_1, SX1280_FLRC_CR_1_2,    32, TLM_RATIO_1_128, 2,  1000, OTA4_PACKET_SIZE, 2},
    {3, RADIO_TYPE_SX128x_FLRC, RATE_DVDA_250HZ,     SX1280_FLRC_BR_0_650_BW_0_6, SX1280_FLRC_BT_1, SX1280_FLRC_CR_1_2,    32, TLM_RATIO_1_128, 2,  1000, OTA4_PACKET_SIZE, 4},
    {4, RADIO_TYPE_SX128x_LORA, RATE_LORA_500HZ,     SX1280_LORA_BW_0800,         SX1280_LORA_SF5,  SX1280_LORA_CR_LI_4_6, 12, TLM_RATIO_1_128, 4,  2000, OTA4_PACKET_SIZE, 1},
    {5, RADIO_TYPE_SX128x_LORA, RATE_LORA_333HZ_8CH, SX1280_LORA_BW_0800,         SX1280_LORA_SF5,  SX1280_LORA_CR_LI_4_8, 12, TLM_RATIO_1_128, 4,  3003, OTA8_PACKET_SIZE, 1},
    {6, RADIO_TYPE_SX128x_LORA, RATE_LORA_250HZ,     SX1280_LORA_BW_0800,         SX1280_LORA_SF6,  SX1280_LORA_CR_LI_4_8, 14, TLM_RATIO_1_64,  4,  4000, OTA4_PACKET_SIZE, 1},
    {7, RADIO_TYPE_SX128x_LORA, RATE_LORA_150HZ,     SX1280_LORA_BW_0800,         SX1280_LORA_SF7,  SX1280_LORA_CR_LI_4_8, 12, TLM_RATIO_1_32,  4,  6666, OTA4_PACKET_SIZE, 1},
    {8, RADIO_TYPE_SX128x_LORA, RATE_LORA_100HZ_8CH, SX1280_LORA_BW_0800,         SX1280_LORA_SF7,  SX1280_LORA_CR_LI_4_8, 12, TLM_RATIO_1_32,  4, 10000, OTA8_PACKET_SIZE, 1},
    {9, RADIO_TYPE_SX128x_LORA, RATE_LORA_50HZ,      SX1280_LORA_BW_0800,         SX1280_LORA_SF8,  SX1280_LORA_CR_LI_4_8, 12, TLM_RATIO_1_16,  2, 20000, OTA4_PACKET_SIZE, 1}};

expresslrs_rf_pref_params_s ExpressLRS_AirRateRFperf[RATE_MAX] = {
    {0, RATE_FLRC_1000HZ,    -104,   389, 2500, 2500,  3, 5000, DYNPOWER_SNR_THRESH_NONE, DYNPOWER_SNR_THRESH_NONE},
    {1, RATE_FLRC_500HZ,     -104,   389, 2500, 2500,  3, 5000, DYNPOWER_SNR_THRESH_NONE, DYNPOWER_SNR_THRESH_NONE},
    {2, RATE_DVDA_500HZ,     -104,   389, 2500, 2500,  3, 5000, DYNPOWER_SNR_THRESH_NONE, DYNPOWER_SNR_THRESH_NONE},
    {3, RATE_DVDA_250HZ,     -104,   389, 2500, 2500,  3, 5000, DYNPOWER_SNR_THRESH_NONE, DYNPOWER_SNR_THRESH_NONE},
    {4, RATE_LORA_500HZ,     -105,  1507, 2500, 2500,  3, 5000, SNR_SCALE( 5), SNR_SCALE(9.5)},
    {5, RATE_LORA_333HZ_8CH, -105,  2374, 2500, 2500,  4, 5000, SNR_SCALE( 5), SNR_SCALE(9.5)},
    {6, RATE_LORA_250HZ,     -108,  3300, 3000, 2500,  6, 5000, SNR_SCALE( 3), SNR_SCALE(9.5)},
    {7, RATE_LORA_150HZ,     -112,  5871, 3500, 2500, 10, 5000, SNR_SCALE( 0), SNR_SCALE(8.5)},
    {8, RATE_LORA_100HZ_8CH, -112,  7605, 3500, 2500, 11, 5000, SNR_SCALE( 0), SNR_SCALE(8.5)},
    {9, RATE_LORA_50HZ,      -115, 10798, 4000, 2500,  0, 5000, SNR_SCALE(-1), SNR_SCALE(6.5)}};
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

uint8_t get_elrs_HandsetRate_max(uint8_t rateIndex, uint32_t minInterval)
{
    while (rateIndex < RATE_MAX)
    {
        expresslrs_mod_settings_s const * const ModParams = &ExpressLRS_AirRateConfig[rateIndex];
        // Handset interval = time between packets from handset, which is expected to be air rate * number of times it is sent
        uint32_t handsetInterval = ModParams->interval * ModParams->numOfSends;
        if (handsetInterval >= minInterval)
            break;
        ++rateIndex;
    }

    return rateIndex;
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

// Connection state information
uint8_t UID[UID_LEN] = {0};  // "bind phrase" ID
bool connectionHasModelMatch = false;
bool teamraceHasModelMatch = true; // true if isTx or teamrace disabled or (enabled and channel in correct postion)
bool InBindingMode = false;
uint8_t ExpressLRS_currTlmDenom = 1;
connectionState_e connectionState = disconnected;
expresslrs_mod_settings_s *ExpressLRS_currAirRate_Modparams = nullptr;
expresslrs_rf_pref_params_s *ExpressLRS_currAirRate_RFperfParams = nullptr;

// Current state of channels, CRSF format
uint32_t ChannelData[CRSF_NUM_CHANNELS];

uint8_t ICACHE_RAM_ATTR TLMratioEnumToValue(expresslrs_tlm_ratio_e const enumval)
{
    // !! TLM_RATIO_STD/TLM_RATIO_DISARMED should be converted by the caller !!
    if (enumval == TLM_RATIO_NO_TLM)
        return 1;

    // 1 << (8 - (enumval - TLM_RATIO_NO_TLM))
    // 1_128 = 128, 1_64 = 64, 1_32 = 32, etc
    return 1 << (8 + TLM_RATIO_NO_TLM - enumval);
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
    // This ^^^ rearranged to preserve precision vvv, using u32 because F1000 1:2 = 256
    unsigned retVal = TELEM_MIN_LINK_INTERVAL_MS * rateHz / ratioDiv / 1000U;

    // Reserve one slot for LINK telemetry. 256 becomes 255 here, safe for return in uint8_t
    if (retVal > 1)
        --retVal;
    else
        retVal = 1;
    //DBGLN("TLMburst: %d", retVal);

    return retVal;
}

uint32_t uidMacSeedGet()
{
    const uint32_t macSeed = ((uint32_t)UID[2] << 24) + ((uint32_t)UID[3] << 16) +
                             ((uint32_t)UID[4] << 8) + (UID[5]^OTA_VERSION_ID);
    return macSeed;
}

bool ICACHE_RAM_ATTR isDualRadio()
{
    return GPIO_PIN_NSS_2 != UNDEF_PIN;
}
