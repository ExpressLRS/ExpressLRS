#include "FHSS.h"
#include "logging.h"
#include "options.h"
#include <string.h>

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

const fhss_config_t domains[] = {
    {"AU915",  FREQ_HZ_TO_REG_VAL(915500000), FREQ_HZ_TO_REG_VAL(926900000), 20, 921000000, 0},
    {"FCC915", FREQ_HZ_TO_REG_VAL(903500000), FREQ_HZ_TO_REG_VAL(926900000), 40, 915000000, 0},
    {"EU868",  FREQ_HZ_TO_REG_VAL(863275000), FREQ_HZ_TO_REG_VAL(869575000), 13, 868000000, 0},
    {"IN866",  FREQ_HZ_TO_REG_VAL(865375000), FREQ_HZ_TO_REG_VAL(866950000), 4, 866000000, 0},
    {"AU433",  FREQ_HZ_TO_REG_VAL(433420000), FREQ_HZ_TO_REG_VAL(434420000), 3, 434000000, 0},
    {"EU433",  FREQ_HZ_TO_REG_VAL(433100000), FREQ_HZ_TO_REG_VAL(434450000), 3, 434000000, 0},
    {"US433",  FREQ_HZ_TO_REG_VAL(433250000), FREQ_HZ_TO_REG_VAL(438000000), 8, 434000000, 0},
    {"US433W", FREQ_HZ_TO_REG_VAL(423500000), FREQ_HZ_TO_REG_VAL(438000000), 20, 434000000, 0},
    // Thailand NBTC 920-925 MHz: 8 FHSS channels, 600 kHz spacing
    {"TH920",  FREQ_HZ_TO_REG_VAL(920500000), FREQ_HZ_TO_REG_VAL(924700000), 8, 922600000, 0},
    {"BR915",  FREQ_HZ_TO_REG_VAL(902400000), FREQ_HZ_TO_REG_VAL(927600000), 42, 915000000, 1, {{9, 21}}},
};

#if defined(RADIO_LR1121) || defined(RADIO_LR2021)
const fhss_config_t domainsDualBand[] = {
    {
    #if defined(Regulatory_Domain_EU_CE_2400)
        "CE_LBT",
    #else
        "ISM2G4",
    #endif
    FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2479400000), 80, 2440000000, 0}
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
    FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2479400000), 80, 2440000000, 0}
};
#endif

// Our table of FHSS frequencies. Define a regulatory domain to select the correct set for your location and radio
const fhss_config_t *FHSSconfig;

// Actual sequence of hops as indexes into the frequency list
uint8_t FHSSsequence[FHSS_SEQUENCE_LEN];

// Which entry in the sequence we currently are on
uint8_t volatile FHSSptr;

// Channel for sync packets and initial connection establishment
uint8_t sync_channel;

// Offset from the predefined frequency determined by AFC on Team900 (register units)
int32_t FreqCorrection;
int32_t FreqCorrection_2;

// Frequency hop separation
uint32_t freq_spread;

uint16_t primaryBandCount;

#if defined(RADIO_LR1121) || defined(RADIO_LR2021) || defined(UNIT_TEST)
// Variables for Dual Band radios
const fhss_config_t *FHSSconfigDualBand;
uint8_t FHSSsequence_DualBand[FHSS_SEQUENCE_LEN];
uint8_t sync_channel_DualBand;
uint32_t freq_spread_DualBand;
bool FHSSusePrimaryFreqBand = true;
bool FHSSuseDualBand = false;
uint16_t secondaryBandCount;
#endif

constexpr uint8_t VERSION_DOMAIN_MAXLEN = 26 + 1;   // max. number of characters (plus '\0') the Lua script can display
                                                    // on color LCD radios w/o being overwritten by the commit info
char version_domain[VERSION_DOMAIN_MAXLEN] {};

static bool isChannelExcluded(uint8_t channel, const fhss_config_t *config)
{
    for (uint8_t i = 0; i < config->excluded_count; i++)
    {
        if (channel >= config->excluded_ranges[i].start && channel <= config->excluded_ranges[i].end)
        {
            return true;
        }
    }
    return false;
}

static uint8_t getUsableChannelCount(const fhss_config_t *config)
{
    uint8_t count = 0;
    for (uint8_t channel = 0; channel < config->freq_count; channel++)
    {
        count += !isChannelExcluded(channel, config);
    }
    return count;
}

static uint8_t getSyncChannel(const fhss_config_t *config)
{
    uint8_t channel = config->freq_count / 2;
    while (isChannelExcluded(channel, config))
    {
        channel = (channel + 1) % config->freq_count;
    }
    return channel;
}

// Convert an index in the compact usable-channel list to a domain channel.
static uint8_t getUsableChannel(const fhss_config_t *config, uint8_t index)
{
    for (uint8_t channel = 0; channel < config->freq_count; channel++)
    {
        if (!isChannelExcluded(channel, config) && index-- == 0)
        {
            return channel;
        }
    }
    return 0;
}

uint8_t FHSSgetGeminiChannel(const fhss_config_t *config, const uint8_t channel)
{
    uint8_t index = 0;
    for (uint8_t candidate = 0; candidate < channel; candidate++)
    {
        index += !isChannelExcluded(candidate, config);
    }

    const uint8_t usableCount = getUsableChannelCount(config);
    return getUsableChannel(config, (index + usableCount / 2) % usableCount);
}

void FHSSrandomiseFHSSsequence(const uint32_t seed)
{
    // the hop pointer indexes sequences that are about to be replaced
    FHSSptr = 0;

    FHSSconfig = &domains[firmwareOptions.domain];
    freq_spread = (FHSSconfig->freq_stop - FHSSconfig->freq_start) * FREQ_SPREAD_SCALE / (FHSSconfig->freq_count - 1);

    sync_channel = getSyncChannel(FHSSconfig);

    const uint8_t usableChannels = getUsableChannelCount(FHSSconfig);

    DBGLN("Primary Domain %s, %u/%u channels, sync=%u",
        FHSSconfig->domain, usableChannels, FHSSconfig->freq_count, sync_channel);

    primaryBandCount = FHSSrandomiseFHSSsequenceBuild(seed, FHSSconfig, sync_channel, FHSSsequence);

#if defined(RADIO_LR1121) || defined(RADIO_LR2021)
    FHSSconfigDualBand = &domainsDualBand[0];
    freq_spread_DualBand = (FHSSconfigDualBand->freq_stop - FHSSconfigDualBand->freq_start) * FREQ_SPREAD_SCALE / (FHSSconfigDualBand->freq_count - 1);

    sync_channel_DualBand = getSyncChannel(FHSSconfigDualBand);

    const uint8_t usableChannelsDual = getUsableChannelCount(FHSSconfigDualBand);

    DBGLN("Dual Domain %s, %u/%u channels, sync=%u",
        FHSSconfigDualBand->domain, usableChannelsDual, FHSSconfigDualBand->freq_count, sync_channel_DualBand);

    secondaryBandCount = FHSSrandomiseFHSSsequenceBuild(seed, FHSSconfigDualBand, sync_channel_DualBand, FHSSsequence_DualBand);
#endif

    // add frequency and regulatory domain to the string used by the Lua script
    addDomainInfo(version_domain, VERSION_DOMAIN_MAXLEN);
}

/**
Requirements:
1. 0 every n hops
2. No two repeated channels
3. Equal occurrence of each (or as even as possible) of each channel
4. Pseudorandom

Approach:
  Fill the sequence array with the sync channel every FHSS_FREQ_CNT
  Iterate through the array, and for each block, swap each entry in it with
  another random entry, excluding the sync channel.

*/
uint16_t FHSSrandomiseFHSSsequenceBuild(const uint32_t seed, const fhss_config_t *config, uint8_t syncChannel, uint8_t *inSequence)
{
    rngSeed(seed);

    const uint8_t usableCount = getUsableChannelCount(config);
    uint8_t syncIndex = 0;
    while (getUsableChannel(config, syncIndex) != syncChannel)
    {
        syncIndex++;
    }

    // initialize the sequence array
    const uint16_t sequenceCount = (FHSS_SEQUENCE_LEN / usableCount) * usableCount;
    for (uint16_t i = 0; i < sequenceCount; i++)
    {
        const uint8_t index = i % usableCount;
        // Keep the sync channel first in every block
        inSequence[i] = index == 0
            ? syncChannel
            : getUsableChannel(config, index == syncIndex ? 0 : index);
    }

    // Randomize each block without moving its first (sync) channel.
    for (uint16_t i = 0; i < sequenceCount; i++)
    {
        // if it's not the sync channel
        if (i % usableCount != 0)
        {
            const uint8_t offset = (i / usableCount) * usableCount; // offset to start of current block
            const uint8_t randomIndex = rngN(usableCount - 1) + 1;  // random number between 1 and the usable frequency count

            // switch this entry and another random entry in the same block
            const uint8_t temp = inSequence[i];
            inSequence[i] = inSequence[offset + randomIndex];
            inSequence[offset + randomIndex] = temp;
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