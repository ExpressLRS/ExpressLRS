#include "common.h"
#include "OTA.h"
#include "FHSS.h"

#ifdef USE_ENCRYPTION
#include <string.h>
#include "encryption.h"
#include "Crypto.h"
#include "ChaCha.h"
#if defined(ESP8266) || defined(ESP32)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif /* defined(ESP8266) || defined(ESP32) */
#endif /* USE_ENCRYPTION */

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
    {0, -112,  4380, 3000, 2500, 600, 5000, SNR_SCALE( 1), SNR_SCALE(3.0)},
    {1, -112,  6690, 3500, 2500, 600, 5000, SNR_SCALE( 1), SNR_SCALE(3.0)},
    {2, -117,  8770, 3500, 2500, 600, 5000, SNR_SCALE( 1), SNR_SCALE(2.5)},
    {3, -120, 18560, 4000, 2500, 600, 5000, SNR_SCALE(-1), SNR_SCALE(1.5)},
    {4, -123, 29950, 6000, 4000, 600, 5000, SNR_SCALE(-3), SNR_SCALE(0.5)},
    {5, -112,  4380, 3000, 2500, 600, 5000, SNR_SCALE( 1), SNR_SCALE(3.0)}};
#endif

#if defined(RADIO_LR1121)

#include "LR1121Driver.h"
LR1121Driver DMA_ATTR Radio;

expresslrs_mod_settings_s ExpressLRS_AirRateConfig[RATE_MAX] = {
    {0,  RADIO_TYPE_LR1121_LORA_900,  RATE_LORA_200HZ,               LR11XX_RADIO_LORA_BW_500,       LR11XX_RADIO_LORA_SF6,    LR11XX_RADIO_LORA_CR_4_7,  8,       LR11XX_RADIO_LORA_BW_500,       LR11XX_RADIO_LORA_SF6,    LR11XX_RADIO_LORA_CR_4_7,  8,  TLM_RATIO_1_64, 4,  5000, OTA4_PACKET_SIZE, 1},
    {1,  RADIO_TYPE_LR1121_LORA_900,  RATE_LORA_100HZ_8CH,           LR11XX_RADIO_LORA_BW_500,       LR11XX_RADIO_LORA_SF6,    LR11XX_RADIO_LORA_CR_4_8,  8,       LR11XX_RADIO_LORA_BW_500,       LR11XX_RADIO_LORA_SF6,    LR11XX_RADIO_LORA_CR_4_8,  8,  TLM_RATIO_1_32, 4, 10000, OTA8_PACKET_SIZE, 1},
    {2,  RADIO_TYPE_LR1121_LORA_900,  RATE_LORA_100HZ,               LR11XX_RADIO_LORA_BW_500,       LR11XX_RADIO_LORA_SF7,    LR11XX_RADIO_LORA_CR_4_7,  8,       LR11XX_RADIO_LORA_BW_500,       LR11XX_RADIO_LORA_SF7,    LR11XX_RADIO_LORA_CR_4_7,  8,  TLM_RATIO_1_32, 4, 10000, OTA4_PACKET_SIZE, 1},
    {3,  RADIO_TYPE_LR1121_LORA_900,  RATE_LORA_50HZ,                LR11XX_RADIO_LORA_BW_500,       LR11XX_RADIO_LORA_SF8,    LR11XX_RADIO_LORA_CR_4_7, 10,       LR11XX_RADIO_LORA_BW_500,       LR11XX_RADIO_LORA_SF8,    LR11XX_RADIO_LORA_CR_4_7, 10,  TLM_RATIO_1_16, 4, 20000, OTA4_PACKET_SIZE, 1},
    {4,  RADIO_TYPE_LR1121_LORA_2G4,  RATE_LORA_500HZ,               LR11XX_RADIO_LORA_BW_800,       LR11XX_RADIO_LORA_SF5, LR11XX_RADIO_LORA_CR_LI_4_6, 12,       LR11XX_RADIO_LORA_BW_800,       LR11XX_RADIO_LORA_SF5, LR11XX_RADIO_LORA_CR_LI_4_6, 12, TLM_RATIO_1_128, 4,  2000, OTA4_PACKET_SIZE, 1},
    {5,  RADIO_TYPE_LR1121_LORA_2G4,  RATE_LORA_333HZ_8CH,           LR11XX_RADIO_LORA_BW_800,       LR11XX_RADIO_LORA_SF5, LR11XX_RADIO_LORA_CR_LI_4_8, 12,       LR11XX_RADIO_LORA_BW_800,       LR11XX_RADIO_LORA_SF5, LR11XX_RADIO_LORA_CR_LI_4_8, 12, TLM_RATIO_1_128, 4,  3003, OTA8_PACKET_SIZE, 1},
    {6,  RADIO_TYPE_LR1121_LORA_2G4,  RATE_LORA_250HZ,               LR11XX_RADIO_LORA_BW_800,       LR11XX_RADIO_LORA_SF6, LR11XX_RADIO_LORA_CR_LI_4_8, 14,       LR11XX_RADIO_LORA_BW_800,       LR11XX_RADIO_LORA_SF6, LR11XX_RADIO_LORA_CR_LI_4_8, 14,  TLM_RATIO_1_64, 4,  4000, OTA4_PACKET_SIZE, 1},
    {7,  RADIO_TYPE_LR1121_LORA_2G4,  RATE_LORA_150HZ,               LR11XX_RADIO_LORA_BW_800,       LR11XX_RADIO_LORA_SF7, LR11XX_RADIO_LORA_CR_LI_4_8, 12,       LR11XX_RADIO_LORA_BW_800,       LR11XX_RADIO_LORA_SF7, LR11XX_RADIO_LORA_CR_LI_4_8, 12,  TLM_RATIO_1_32, 4,  6666, OTA4_PACKET_SIZE, 1},
    {8,  RADIO_TYPE_LR1121_LORA_2G4,  RATE_LORA_100HZ_8CH,           LR11XX_RADIO_LORA_BW_800,       LR11XX_RADIO_LORA_SF7, LR11XX_RADIO_LORA_CR_LI_4_8, 12,       LR11XX_RADIO_LORA_BW_800,       LR11XX_RADIO_LORA_SF7, LR11XX_RADIO_LORA_CR_LI_4_8, 12,  TLM_RATIO_1_32, 4, 10000, OTA8_PACKET_SIZE, 1},
    {9,  RADIO_TYPE_LR1121_LORA_2G4,  RATE_LORA_50HZ,                LR11XX_RADIO_LORA_BW_800,       LR11XX_RADIO_LORA_SF8, LR11XX_RADIO_LORA_CR_LI_4_8, 12,       LR11XX_RADIO_LORA_BW_800,       LR11XX_RADIO_LORA_SF8, LR11XX_RADIO_LORA_CR_LI_4_8, 12,  TLM_RATIO_1_16, 2, 20000, OTA4_PACKET_SIZE, 1},
    {10, RADIO_TYPE_LR1121_LORA_DUAL, RATE_LORA_150HZ,               LR11XX_RADIO_LORA_BW_500,       LR11XX_RADIO_LORA_SF6,    LR11XX_RADIO_LORA_CR_4_8, 12,       LR11XX_RADIO_LORA_BW_800,       LR11XX_RADIO_LORA_SF7, LR11XX_RADIO_LORA_CR_LI_4_6, 12,  TLM_RATIO_1_32, 4,  6666, OTA4_PACKET_SIZE, 1},
    {11, RADIO_TYPE_LR1121_LORA_DUAL, RATE_LORA_100HZ_8CH,           LR11XX_RADIO_LORA_BW_500,       LR11XX_RADIO_LORA_SF6,    LR11XX_RADIO_LORA_CR_4_8, 18,       LR11XX_RADIO_LORA_BW_800,       LR11XX_RADIO_LORA_SF7, LR11XX_RADIO_LORA_CR_LI_4_8, 14,  TLM_RATIO_1_32, 4, 10000, OTA8_PACKET_SIZE, 1},
    {12, RADIO_TYPE_LR1121_LORA_900,  RATE_LORA_250HZ,               LR11XX_RADIO_LORA_BW_500,       LR11XX_RADIO_LORA_SF5,    LR11XX_RADIO_LORA_CR_4_8,  8,       LR11XX_RADIO_LORA_BW_500,       LR11XX_RADIO_LORA_SF5,    LR11XX_RADIO_LORA_CR_4_8,  8,  TLM_RATIO_1_64, 4,  4000, OTA4_PACKET_SIZE, 1},
    {13, RADIO_TYPE_LR1121_LORA_900,  RATE_LORA_200HZ_8CH,           LR11XX_RADIO_LORA_BW_500,       LR11XX_RADIO_LORA_SF5,    LR11XX_RADIO_LORA_CR_4_7,  8,       LR11XX_RADIO_LORA_BW_500,       LR11XX_RADIO_LORA_SF5,    LR11XX_RADIO_LORA_CR_4_7,  8,  TLM_RATIO_1_64, 4,  5000, OTA8_PACKET_SIZE, 1},
    {14, RADIO_TYPE_LR1121_GFSK_2G4,  RATE_FSK_2G4_DVDA_500HZ, LR11XX_RADIO_GFSK_BITRATE_300k, LR11XX_RADIO_GFSK_BW_467000, LR11XX_RADIO_GFSK_FDEV_100k, 16, LR11XX_RADIO_GFSK_BITRATE_300k, LR11XX_RADIO_GFSK_BW_467000, LR11XX_RADIO_GFSK_FDEV_100k, 16, TLM_RATIO_1_128, 2,  1000, OTA4_PACKET_SIZE, 2},
    {15, RADIO_TYPE_LR1121_GFSK_900,  RATE_FSK_900_1000HZ_8CH, LR11XX_RADIO_GFSK_BITRATE_300k, LR11XX_RADIO_GFSK_BW_467000, LR11XX_RADIO_GFSK_FDEV_100k, 16, LR11XX_RADIO_GFSK_BITRATE_300k, LR11XX_RADIO_GFSK_BW_467000, LR11XX_RADIO_GFSK_FDEV_100k, 16, TLM_RATIO_1_128, 2,  1000, OTA8_PACKET_SIZE, 1}};

expresslrs_rf_pref_params_s ExpressLRS_AirRateRFperf[RATE_MAX] = {
    {0,  -112,  4380, 3000, 2500, 600,  5000, SNR_SCALE( 1), SNR_SCALE(3.0)}, // These SNR_SCALE values all need to be checked!
    {1,  -112,  6690, 3500, 2500, 600,  5000, SNR_SCALE( 1), SNR_SCALE(3.0)},
    {2,  -117,  8770, 3500, 2500, 600,  5000, SNR_SCALE( 1), SNR_SCALE(2.5)},
    {3,  -120, 18560, 4000, 2500, 600,  5000, SNR_SCALE(-1), SNR_SCALE(1.5)},
    {4,  -105,  1507, 2500, 2500,   3,  5000, SNR_SCALE( 5), SNR_SCALE(9.5)},
    {5,  -105,  2374, 2500, 2500,   4,  5000, SNR_SCALE( 5), SNR_SCALE(9.5)},
    {6,  -108,  3300, 3000, 2500,   6,  5000, SNR_SCALE( 3), SNR_SCALE(9.5)},
    {7,  -112,  5871, 3500, 2500,  10,  5000, SNR_SCALE( 0), SNR_SCALE(8.5)},
    {8,  -112,  7605, 3500, 2500,  11,  5000, SNR_SCALE( 0), SNR_SCALE(8.5)},
    {9,  -115, 10798, 4000, 2500,   0,  5000, SNR_SCALE(-1), SNR_SCALE(6.5)},
    {10, -112,  5871, 3500, 2500,  10,  5000, SNR_SCALE( 0), SNR_SCALE(8.5)},
    {11, -112,  7456, 3500, 2500,  11,  5000, SNR_SCALE( 0), SNR_SCALE(8.5)},
    {12, -111,  3216, 3500, 2500, 600,  5000, SNR_SCALE( 1), SNR_SCALE(3.0)},
    {13, -111,  4240, 3500, 2500, 600,  5000, SNR_SCALE( 1), SNR_SCALE(3.0)},
    {14, -103,   690, 2500, 2500,   3,  5000, DYNPOWER_SNR_THRESH_NONE, DYNPOWER_SNR_THRESH_NONE},
    {15, -101,   658, 2500, 2500,   3,  5000, DYNPOWER_SNR_THRESH_NONE, DYNPOWER_SNR_THRESH_NONE}};
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
    {0, -104,   389, 2500, 2500,  3, 5000, DYNPOWER_SNR_THRESH_NONE, DYNPOWER_SNR_THRESH_NONE},
    {1, -104,   389, 2500, 2500,  3, 5000, DYNPOWER_SNR_THRESH_NONE, DYNPOWER_SNR_THRESH_NONE},
    {2, -104,   389, 2500, 2500,  3, 5000, DYNPOWER_SNR_THRESH_NONE, DYNPOWER_SNR_THRESH_NONE},
    {3, -104,   389, 2500, 2500,  3, 5000, DYNPOWER_SNR_THRESH_NONE, DYNPOWER_SNR_THRESH_NONE},
    {4, -105,  1507, 2500, 2500,  3, 5000, SNR_SCALE( 5), SNR_SCALE(9.5)},
    {5, -105,  2374, 2500, 2500,  4, 5000, SNR_SCALE( 5), SNR_SCALE(9.5)},
    {6, -108,  3300, 3000, 2500,  6, 5000, SNR_SCALE( 3), SNR_SCALE(9.5)},
    {7, -112,  5871, 3500, 2500, 10, 5000, SNR_SCALE( 0), SNR_SCALE(8.5)},
    {8, -112,  7605, 3500, 2500, 11, 5000, SNR_SCALE( 0), SNR_SCALE(8.5)},
    {9, -115, 10798, 4000, 2500,  0, 5000, SNR_SCALE(-1), SNR_SCALE(6.5)}};
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
uint8_t UID[UID_LEN] = {1};  // "bind phrase" ID
uint8_t bindPhrase[PHRASE_LEN] = {0}; // "bind phrase" ID
bool connectionHasModelMatch = false;
bool teamraceHasModelMatch = true; // true if isTx or teamrace disabled or (enabled and channel in correct postion)
bool InBindingMode = false;
bool gettingSerialIn = false;
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

#ifdef USE_ENCRYPTION
extern ChaCha cipher;

// TODO: There has to be a better way of passing the params.
// They're all from the same struct but don't always get passed in from the otaPkt
void ICACHE_RAM_ATTR encryptMsg(uint8_t *output, uint8_t *input, size_t length, uint8_t fhss, uint8_t otaNonce, uint8_t crcLow)
{
    //const uint8_t counter[] = {FHSSsequence[FHSSptr], 110, input[0], 112, input[length - 1], 114, OtaNonce, 116};
    const uint8_t nonce[] = {fhss, 110, 111, crcLow, 113, 114, otaNonce, 116};
    cipher.setIV(nonce, sizeof(nonce));
    cipher.encrypt(output + 1, input + 1, length - 2);
}

void ICACHE_RAM_ATTR decryptMsg(uint8_t *output, uint8_t *input, size_t length, uint8_t fhss, uint8_t otaNonce)
{
    uint8_t crcLow = input[length - 1];
    encryptMsg(output, input, length, fhss, otaNonce, crcLow);
}

bool initCrypto()
{
  constexpr uint8_t rounds = 12; // just like a boxing match
  constexpr size_t keySize = 16;
  constexpr size_t nonceSize = 8;

  uint8_t nonce[nonceSize] = {0};
  uint8_t key[keySize] = {0};

  cipher.clear();

  for (size_t i = 0; i < keySize; ++i) {
    key[i] = (i < UID_LEN) ? UID[i] : (i + 1);
  }

  cipher.setNumRounds(rounds);
  if (!cipher.setKey(key, keySize)) {
    DBGLN("ERR: Failed to set cipher key. master_key: %s. keySize: %lu", key, keySize);
    return false;
  }
  // This code is here ONLY to check that our assumed nonceSize is supported by the cipher.
  // Actual nonce will be set before encrypting/decrypting each message.
  if (!cipher.setIV(nonce, nonceSize)) {
    DBGLN("ERR: Failed to set cipher nonce. size: %lu", cipher.ivSize())
    return false;
  }

  return true;
}

int hexStr2Arr(uint8_t *output, const char *input, size_t out_len_max)
{
    if(!out_len_max)
        out_len_max = INT_MAX;

    size_t in_len = strnlen(input, out_len_max * 2);
    if(in_len % 2 != 0)
        in_len--;

    /* calc actual out len */
    const size_t out_len = out_len_max < (in_len / 2) ? out_len_max : (in_len / 2);

    for(size_t i = 0; i < out_len; i++)
    {
        char ch0 = input[i * 2];
        char ch1 = input[i * 2 + 1];
        uint8_t nib0 = ( (ch0 & 0xF) + (ch0 >> 6) ) | ((ch0 >> 3) & 0x8);
        uint8_t nib1 = ( (ch1 & 0xF) + (ch1 >> 6) ) | ((ch1 >> 3) & 0x8);
        output[i] = (nib0 << 4) | nib1;
    }
    return out_len;
}
#endif