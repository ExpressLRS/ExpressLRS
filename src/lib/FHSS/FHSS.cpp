#include "FHSS.h"
#include "logging.h"
#include "options.h"
#include <string.h>
#include <stdlib.h>

#if defined(UNIT_TEST)
#define POWER_OUTPUT_VALUES_COUNT 4
#define POWER_OUTPUT_VALUES_DUAL_COUNT 0
#endif

#if defined(RADIO_SX127X) || defined(RADIO_LR1121) || defined(RADIO_LR2021)

#if defined(RADIO_SX127X)
#include "SX127xDriver.h"
#elif defined(RADIO_LR1121)
#include "LR1121Driver.h"
#else
#include "LR2021Driver.h"
#endif

// Preserve the CE sub-GHz cap on dual-band radios. Other builds keep their
// existing hardware-defined maximum until a domain specifies a tighter limit.
#if defined(Regulatory_Domain_EU_CE_2400) && (defined(RADIO_LR1121) || defined(RADIO_LR2021))
constexpr PowerLevels_e SUBGHZ_MAX_POWER = PWR_25mW;
#else
constexpr PowerLevels_e SUBGHZ_MAX_POWER = PWR_2000mW;
#endif

const fhss_config_t domains[] = {
    {"AU915",  FREQ_HZ_TO_REG_VAL(915500000), FREQ_HZ_TO_REG_VAL(926900000), 20, 921000000, SUBGHZ_MAX_POWER, nullptr, 0},
    {"FCC915", FREQ_HZ_TO_REG_VAL(903500000), FREQ_HZ_TO_REG_VAL(926900000), 40, 915000000, SUBGHZ_MAX_POWER, nullptr, 0},
    {"EU868",  FREQ_HZ_TO_REG_VAL(863275000), FREQ_HZ_TO_REG_VAL(869575000), 13, 868000000, SUBGHZ_MAX_POWER, nullptr, 0},
    {"IN866",  FREQ_HZ_TO_REG_VAL(865375000), FREQ_HZ_TO_REG_VAL(866950000), 4, 866000000, SUBGHZ_MAX_POWER, nullptr, 0},
    {"AU433",  FREQ_HZ_TO_REG_VAL(433420000), FREQ_HZ_TO_REG_VAL(434420000), 3, 434000000, SUBGHZ_MAX_POWER, nullptr, 0},
    {"EU433",  FREQ_HZ_TO_REG_VAL(433100000), FREQ_HZ_TO_REG_VAL(434450000), 3, 434000000, SUBGHZ_MAX_POWER, nullptr, 0},
    {"US433",  FREQ_HZ_TO_REG_VAL(433250000), FREQ_HZ_TO_REG_VAL(438000000), 8, 434000000, SUBGHZ_MAX_POWER, nullptr, 0},
    {"US433W",  FREQ_HZ_TO_REG_VAL(423500000), FREQ_HZ_TO_REG_VAL(438000000), 20, 434000000, SUBGHZ_MAX_POWER, nullptr, 0},
    // Thailand NBTC 920-925 MHz: 8 FHSS channels, 600 kHz spacing
    {"TH920",  FREQ_HZ_TO_REG_VAL(920500000), FREQ_HZ_TO_REG_VAL(924700000), 8, 922600000, SUBGHZ_MAX_POWER, nullptr, 0},
};

#if defined(RADIO_LR1121) || defined(RADIO_LR2021)
const fhss_config_t domainsDualBand[] = {
    {
    #if defined(Regulatory_Domain_EU_CE_2400)
        "CE_LBT",
    #else
        "ISM2G4",
    #endif
    FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2479400000), 80, 2440000000,
    #if defined(Regulatory_Domain_EU_CE_2400)
        PWR_100mW,
    #else
        PWR_2000mW,
    #endif
    nullptr, 0
    }
};
#endif

#elif defined(RADIO_SX128X)
#include "SX1280Driver.h"

const fhss_config_t domains[] = {
    {
    #if defined(Regulatory_Domain_EU_CE_2400)
        "CE_LBT",
    #elif defined(Regulatory_Domain_ISM_2400)
        "ISM2G4",
    #endif
    FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2479400000), 80, 2440000000,
    #if defined(Regulatory_Domain_EU_CE_2400)
        PWR_100mW,
    #else
        PWR_2000mW,
    #endif
    nullptr, 0
    }
};
#endif

// Our table of FHSS frequencies. Define a regulatory domain to select the correct set for your location and radio
const fhss_config_t *FHSSconfig;

const fhss_config_t &FHSSgetPrimaryConfig()
{
    return domains[firmwareOptions.domain];
}

const fhss_config_t &FHSSgetSecondaryConfig()
{
#if defined(RADIO_LR1121) || defined(RADIO_LR2021)
    return domainsDualBand[0];
#else
    return FHSSgetPrimaryConfig();
#endif
}

// Actual sequence of hops as indexes into the frequency list
uint8_t FHSSsequence[FHSS_SEQUENCE_LEN];

// Which entry in the sequence we currently are on
uint8_t volatile FHSSptr;

// Channel for sync packets and initial connection establishment
uint_fast8_t sync_channel;

// Offset from the predefined frequency determined by AFC on Team900 (register units)
int32_t FreqCorrection;
int32_t FreqCorrection_2;

// Frequency hop separation
uint32_t freq_spread;

uint16_t primaryBandCount;
uint32_t primaryFreqCount;
uint8_t FHSSchannelMap[FHSS_MAX_FREQ_COUNT];

#if defined(RADIO_LR1121) || defined(RADIO_LR2021) || defined(UNIT_TEST)
// Variables for Dual Band radios
const fhss_config_t *FHSSconfigDualBand;
uint8_t FHSSsequence_DualBand[FHSS_SEQUENCE_LEN];
uint_fast8_t sync_channel_DualBand;
uint32_t freq_spread_DualBand;
bool FHSSusePrimaryFreqBand = true;
bool FHSSuseDualBand = false;
uint16_t secondaryBandCount;
uint32_t secondaryFreqCount;
uint8_t FHSSchannelMap_DualBand[FHSS_MAX_FREQ_COUNT];
#endif

constexpr uint8_t VERSION_DOMAIN_MAXLEN = 26 + 1;   // max. number of characters (plus '\0') the Lua script can display
                                                    // on color LCD radios w/o being overwritten by the commit info
char version_domain[VERSION_DOMAIN_MAXLEN] {};


uint32_t FHSSinitialiseBand(const fhss_config_t &config, uint8_t *channelMap,
                           uint_fast8_t &syncChannel, uint32_t &spread)
{
    if (config.freq_count < 2 || config.freq_count > FHSS_MAX_FREQ_COUNT ||
        (config.exclusion_count && !config.exclusions))
    {
        return 0;
    }
    for (uint8_t i = 0; i < config.exclusion_count; ++i)
    {
        const fhss_exclusion_t &range = config.exclusions[i];
        if (range.first > range.last || range.last >= config.freq_count)
            return 0;
    }

    uint32_t count = 0;
    for (uint32_t channel = 0; channel < config.freq_count; ++channel)
    {
        bool excluded = false;
        for (uint8_t i = 0; i < config.exclusion_count; ++i)
        {
            const fhss_exclusion_t &range = config.exclusions[i];
            if (channel >= range.first && channel <= range.last)
            {
                excluded = true;
                break;
            }
        }
        if (!excluded)
            channelMap[count++] = channel;
    }
    if (count < 2)
        return 0;
    syncChannel = count / 2;
    // Keep the original grid spacing, not the spacing of a compressed range.
    spread = (config.freq_stop - config.freq_start) * FREQ_SPREAD_SCALE / (config.freq_count - 1);
    return count;
}

void FHSSrandomiseFHSSsequence(const uint32_t seed)
{
    // the hop pointer indexes sequences that are about to be replaced
    FHSSptr = 0;

    FHSSconfig = &domains[firmwareOptions.domain];
    primaryFreqCount = FHSSinitialiseBand(*FHSSconfig, FHSSchannelMap, sync_channel, freq_spread);
    if (!primaryFreqCount)
        abort(); // Invalid domain metadata must never enable excluded frequencies.

    DBGLN("Primary Domain %s, %u channels, sync=%u",
        FHSSconfig->domain, primaryFreqCount, sync_channel);

    primaryBandCount = FHSSrandomiseFHSSsequenceBuild(seed, primaryFreqCount, sync_channel, FHSSsequence);

#if defined(RADIO_LR1121) || defined(RADIO_LR2021)
    FHSSconfigDualBand = &domainsDualBand[0];
    secondaryFreqCount = FHSSinitialiseBand(*FHSSconfigDualBand, FHSSchannelMap_DualBand,
        sync_channel_DualBand, freq_spread_DualBand);
    if (!secondaryFreqCount)
        abort();

    DBGLN("Dual Domain %s, %u channels, sync=%u",
        FHSSconfigDualBand->domain, secondaryFreqCount, sync_channel_DualBand);

    secondaryBandCount = FHSSrandomiseFHSSsequenceBuild(seed, secondaryFreqCount, sync_channel_DualBand, FHSSsequence_DualBand);
#endif

    // add frequency and regulatory domain to the string used by the Lua script
    addDomainInfo(version_domain, VERSION_DOMAIN_MAXLEN);
}

/**
Requirements:
1. 0 every n hops
2. No two repeated channels
3. Equal occurance of each (or as even as possible) of each channel
4. Pseudorandom

Approach:
  Fill the sequence array with the sync channel every FHSS_FREQ_CNT
  Iterate through the array, and for each block, swap each entry in it with
  another random entry, excluding the sync channel.

*/
uint16_t FHSSrandomiseFHSSsequenceBuild(const uint32_t seed, uint32_t freqCount, uint_fast8_t syncChannel, uint8_t *inSequence)
{
    const uint16_t sequenceCount = (FHSS_SEQUENCE_LEN / freqCount) * freqCount;

    rngSeed(seed);

    // initialize the sequence array
    for (uint16_t i = 0; i < sequenceCount; i++)
    {
        if (i % freqCount == 0) {
            inSequence[i] = syncChannel;
        } else if (i % freqCount == syncChannel) {
            inSequence[i] = 0;
        } else {
            inSequence[i] = i % freqCount;
        }
    }

    for (uint16_t i = 0; i < sequenceCount; i++)
    {
        // if it's not the sync channel
        if (i % freqCount != 0)
        {
            uint8_t offset = (i / freqCount) * freqCount;   // offset to start of current block
            uint8_t rand = rngN(freqCount - 1) + 1;         // random number between 1 and FHSS_FREQ_CNT

            // switch this entry and another random entry in the same block
            uint8_t temp = inSequence[i];
            inSequence[i] = inSequence[offset+rand];
            inSequence[offset+rand] = temp;
        }
    }

    // output FHSS sequence
    // for (uint16_t i=0; i < sequenceCount; i++)
    // {
    //     DBG("%u ",inSequence[i]);
    //     if (i % 10 == 9)
    //         DBGCR;
    // }
    // DBGCR;

    return sequenceCount;
}

/**
 * @brief Add frequency and regulatory domain to the version string used by the Lua script. Outputs the version_domain string as:
 * [version:0..20] [subGHz domain | 2.4GHz domain] truncated to maxlen-1 for single band devices
 * [version:0..20] [subGHz domain]/[2.4GHz domain] truncated to maxlen-1 for dual band devices
 * Examples:
 *   4.0.0 CE_LBT
 *   4.1.7 AU915
 *   4.11.17 FCC915/ISM2G4
 *   someBranch EU868/CE_LBT
 *
 * @param version_domain a pointer to a buffer holding the version and extra space for additional data
 * @param maxlen the size of the provided buffer
 */
void addDomainInfo(char *version_domain, uint8_t maxlen)
{
    if (strlen(version) < 21)
    {
        strlcpy(version_domain, version, 21);
        strlcat(version_domain, " ", maxlen);
    }
    else
    {
        strlcpy(version_domain, version, 18);
        strlcat(version_domain, "... ", maxlen);
    }

    if (POWER_OUTPUT_VALUES_COUNT != 0)
    {
        strlcat(version_domain, FHSSconfig->domain, maxlen);            // single band: subghz or 2.4GHz, dual band: subghz
    }
    if (POWER_OUTPUT_VALUES_COUNT != 0 && POWER_OUTPUT_VALUES_DUAL_COUNT != 0)
    {
        strlcat(version_domain, "/", maxlen);
    }
    if (POWER_OUTPUT_VALUES_DUAL_COUNT != 0)
    {
        strlcat(version_domain, FHSSconfigDualBand->domain, maxlen);    // 2.4GHz
    }
}

bool isUsingPrimaryFreqBand()
{
    return FHSSusePrimaryFreqBand;
}
