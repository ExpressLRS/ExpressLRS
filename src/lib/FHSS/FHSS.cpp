#include "FHSS.h"
#include "logging.h"
#include "options.h"
#include <string.h>

#if defined(UNIT_TEST)
#define POWER_OUTPUT_VALUES_COUNT 4
#define POWER_OUTPUT_VALUES_DUAL_COUNT 0
#endif

#if defined(RADIO_MULTI)
// Each chip has its own frequency unit: a register step on the SX12xx chips, Hz on the LR
// ones. Each table below is converted with its own chip's: FREQ_HZ_TO_REG_VAL() uses
// FREQ_STEP, set to the SX127x's for the first sub-GHz table and the SX1280's for 2.4 GHz.
#include "MultiRadioDriver.h"
#if defined(RADIO_LR1121) || defined(RADIO_LR2021)
#define FREQ_HZ_TO_REG_VAL(freq) ((uint32_t)((double)freq/(double)FREQ_STEP))
#endif
#define FREQ_STEP SX127X_FREQ_STEP
#endif

#if defined(RADIO_SX127X) || defined(RADIO_LR1121) || defined(RADIO_LR2021)

#if defined(RADIO_MULTI)
// MultiRadioDriver.h has the drivers
#elif defined(RADIO_SX127X)
#include "SX127xDriver.h"
#elif defined(RADIO_LR1121)
#include "LR1121Driver.h"
#elif defined(RADIO_LR2021)
#include "LR2021Driver.h"
#endif

// A macro so that a multi-radio build can expand it once per frequency unit
#define FHSS_DOMAINS_900 \
    {"AU915",  FREQ_HZ_TO_REG_VAL(915500000), FREQ_HZ_TO_REG_VAL(926900000), 20, 921000000}, \
    {"FCC915", FREQ_HZ_TO_REG_VAL(903500000), FREQ_HZ_TO_REG_VAL(926900000), 40, 915000000}, \
    {"EU868",  FREQ_HZ_TO_REG_VAL(863275000), FREQ_HZ_TO_REG_VAL(869575000), 13, 868000000}, \
    {"IN866",  FREQ_HZ_TO_REG_VAL(865375000), FREQ_HZ_TO_REG_VAL(866950000), 4, 866000000}, \
    {"AU433",  FREQ_HZ_TO_REG_VAL(433420000), FREQ_HZ_TO_REG_VAL(434420000), 3, 434000000}, \
    {"EU433",  FREQ_HZ_TO_REG_VAL(433100000), FREQ_HZ_TO_REG_VAL(434450000), 3, 434000000}, \
    {"US433",  FREQ_HZ_TO_REG_VAL(433250000), FREQ_HZ_TO_REG_VAL(438000000), 8, 434000000}, \
    {"US433W",  FREQ_HZ_TO_REG_VAL(423500000), FREQ_HZ_TO_REG_VAL(438000000), 20, 434000000}, \
    /* Thailand NBTC 920-925 MHz: 8 FHSS channels, 600 kHz spacing */ \
    {"TH920",  FREQ_HZ_TO_REG_VAL(920500000), FREQ_HZ_TO_REG_VAL(924700000), 8, 922600000},

const fhss_config_t domains[] = {
    FHSS_DOMAINS_900
};

#if (defined(RADIO_LR1121) || defined(RADIO_LR2021)) && !defined(RADIO_MULTI)
// the same channels either way: firmwareOptions.lbt picks which one names them
const fhss_config_t domainsDualBand[] = {
    {"ISM2G4", FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2479400000), 80, 2440000000},
    {"CE_LBT", FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2479400000), 80, 2440000000},
};
#endif

#endif

#if defined(RADIO_SX128X)
#if defined(RADIO_MULTI)
#undef FREQ_STEP
#define FREQ_STEP SX1280_FREQ_STEP
// domains[] is the sub-GHz table above
#define DOMAINS_2G4 domains2G4
#else
#include "SX1280Driver.h"
#define DOMAINS_2G4 domains
#endif

// the same channels either way: firmwareOptions.lbt picks which one names them
const fhss_config_t DOMAINS_2G4[] = {
    {"ISM2G4", FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2479400000), 80, 2440000000},
    {"CE_LBT", FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2479400000), 80, 2440000000},
};
#endif

#if defined(RADIO_MULTI) && (defined(RADIO_LR1121) || defined(RADIO_LR2021))
// the LR chips take Hz: the same domains again, unconverted
#undef FREQ_HZ_TO_REG_VAL
#define FREQ_HZ_TO_REG_VAL(freq) (freq)
static const fhss_config_t domainsHz[] = {
    FHSS_DOMAINS_900
};

static const fhss_config_t domainsDualBand[] = {
    {"ISM2G4", FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2479400000), 80, 2440000000},
    {"CE_LBT", FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2479400000), 80, 2440000000},
};

// 8 extra bits of spread precision on the SX12xx register units; none on Hz, which would overflow
uint8_t freqSpreadShift = 8;
#endif

// Our table of FHSS frequencies. Define a regulatory domain to select the correct set for your location and radio
const fhss_config_t *FHSSconfig;

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

#if defined(RADIO_LR1121) || defined(RADIO_LR2021) || defined(UNIT_TEST)
// Variables for Dual Band radios
const fhss_config_t *FHSSconfigDualBand;
uint8_t FHSSsequence_DualBand[FHSS_SEQUENCE_LEN];
uint_fast8_t sync_channel_DualBand;
uint32_t freq_spread_DualBand;
bool FHSSusePrimaryFreqBand = true;
bool FHSSuseDualBand = false;
uint16_t secondaryBandCount;
#endif

constexpr uint8_t VERSION_DOMAIN_MAXLEN = 26 + 1;   // max. number of characters (plus '\0') the Lua script can display
                                                    // on color LCD radios w/o being overwritten by the commit info
char version_domain[VERSION_DOMAIN_MAXLEN] {};


void FHSSrandomiseFHSSsequence(const uint32_t seed)
{
    // the hop pointer indexes sequences that are about to be replaced
    FHSSptr = 0;

#if defined(RADIO_MULTI) && (defined(RADIO_LR1121) || defined(RADIO_LR2021))
    // the 2.4 GHz band has one domain, which the sub-GHz domain option doesn't select
    if (radioIsLR())
    {
        FHSSconfig = &domainsHz[firmwareOptions.domain];
        freqSpreadShift = 0;
    }
    else
    {
        FHSSconfig = radioChip == RADIO_CHIP_SX127X ? &domains[firmwareOptions.domain] : &domains2G4[firmwareOptions.lbt ? 1 : 0];
        freqSpreadShift = 8;
    }
#elif defined(RADIO_MULTI)
    // the 2.4 GHz band has one domain, which the sub-GHz domain option doesn't select
    FHSSconfig = radioChip == RADIO_CHIP_SX127X ? &domains[firmwareOptions.domain] : &domains2G4[firmwareOptions.lbt ? 1 : 0];
#elif defined(RADIO_SX128X)
    FHSSconfig = &domains[firmwareOptions.lbt ? 1 : 0];
#else
    FHSSconfig = &domains[firmwareOptions.domain];
#endif
    sync_channel = FHSSconfig->freq_count / 2;
    freq_spread = (FHSSconfig->freq_stop - FHSSconfig->freq_start) * FREQ_SPREAD_SCALE / (FHSSconfig->freq_count - 1);

    DBGLN("Primary Domain %s, %u channels, sync=%u",
        FHSSconfig->domain, FHSSconfig->freq_count, sync_channel);

    primaryBandCount = FHSSrandomiseFHSSsequenceBuild(seed, FHSSconfig->freq_count, sync_channel, FHSSsequence);

#if defined(RADIO_LR1121) || defined(RADIO_LR2021)
#if defined(RADIO_MULTI)
    // an SX12xx chip has one band: its rates never leave FHSSconfig
    if (radioIsLR())
#endif
    {
        FHSSconfigDualBand = &domainsDualBand[firmwareOptions.lbt ? 1 : 0];
        sync_channel_DualBand = FHSSconfigDualBand->freq_count / 2;
        freq_spread_DualBand = (FHSSconfigDualBand->freq_stop - FHSSconfigDualBand->freq_start) * FREQ_SPREAD_SCALE / (FHSSconfigDualBand->freq_count - 1);

        DBGLN("Dual Domain %s, %u channels, sync=%u",
            FHSSconfigDualBand->domain, FHSSconfigDualBand->freq_count, sync_channel_DualBand);

        secondaryBandCount = FHSSrandomiseFHSSsequenceBuild(seed, FHSSconfigDualBand->freq_count, sync_channel_DualBand, FHSSsequence_DualBand);
    }
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