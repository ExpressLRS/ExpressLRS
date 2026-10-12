#pragma once

#include "targets.h"
#include "random.h"
#include "PowerLevels.h"

#if defined(RADIO_SX127X)
#define FreqCorrectionMax ((int32_t)(100000/FREQ_STEP))
#elif defined(RADIO_SX128X)
#define FreqCorrectionMax ((int32_t)(200000/FREQ_STEP))
#elif defined(RADIO_LR1121)
#define FreqCorrectionMax ((int32_t)(100000/FREQ_STEP)) // TODO - This needs checking !!!
#elif defined(RADIO_LR2021)
#define FreqCorrectionMax ((int32_t)(100000/FREQ_STEP)) // TODO - This needs checking !!!
#endif
#define FreqCorrectionMin (-FreqCorrectionMax)

#if defined(RADIO_LR1121) || defined(RADIO_LR2021) || (defined(UNIT_TEST) && !defined(RADIO_SX127X))
#define FREQ_HZ_TO_REG_VAL(freq) (freq)
#define FREQ_SPREAD_SCALE 1
#else
#define FREQ_HZ_TO_REG_VAL(freq) ((uint32_t)((double)freq/(double)FREQ_STEP))
#define FREQ_SPREAD_SCALE 256
#endif

#define FHSS_SEQUENCE_LEN 256
#define FHSS_MAX_FREQ_COUNT 80

struct fhss_exclusion_t {
    uint8_t first;
    uint8_t last; // inclusive indexes into the original regular grid
};

typedef struct {
    const char  *domain;
    uint32_t    freq_start;
    uint32_t    freq_stop;
    uint32_t    freq_count;
    uint32_t    freq_center;
    PowerLevels_e max_power;
    const fhss_exclusion_t *exclusions;
    uint8_t exclusion_count;
} fhss_config_t;

extern volatile uint8_t FHSSptr;
extern int32_t FreqCorrection;      // Only used for the SX1276
extern int32_t FreqCorrection_2;    // Only used for the SX1276

// Primary Band
extern uint16_t primaryBandCount;
extern uint32_t freq_spread;
extern uint8_t FHSSsequence[];
extern uint_fast8_t sync_channel;
extern const fhss_config_t *FHSSconfig;
extern uint8_t FHSSchannelMap[FHSS_MAX_FREQ_COUNT];
extern uint32_t primaryFreqCount;

// DualBand Variables
#if defined(RADIO_LR1121) || defined(RADIO_LR2021) || defined(UNIT_TEST)
extern bool FHSSusePrimaryFreqBand;
extern bool FHSSuseDualBand;
extern uint16_t secondaryBandCount;
extern uint32_t freq_spread_DualBand;
extern uint8_t FHSSsequence_DualBand[];
extern uint_fast8_t sync_channel_DualBand;
extern const fhss_config_t *FHSSconfigDualBand;
extern uint8_t FHSSchannelMap_DualBand[FHSS_MAX_FREQ_COUNT];
extern uint32_t secondaryFreqCount;
#else
// Single band radios never leave the primary band. The constants let the
// compiler drop the dual band branches below, and the aliases keep those
// branches compiling without a second sequence table in RAM.
constexpr bool FHSSusePrimaryFreqBand = true;
constexpr bool FHSSuseDualBand = false;
constexpr uint16_t secondaryBandCount = 0;
[[maybe_unused]] static uint32_t &freq_spread_DualBand = freq_spread;
[[maybe_unused]] static uint8_t *const FHSSsequence_DualBand = FHSSsequence;
[[maybe_unused]] static uint_fast8_t &sync_channel_DualBand = sync_channel;
[[maybe_unused]] static const fhss_config_t *&FHSSconfigDualBand = FHSSconfig;
[[maybe_unused]] static uint8_t *const FHSSchannelMap_DualBand = FHSSchannelMap;
[[maybe_unused]] static uint32_t &secondaryFreqCount = primaryFreqCount;
#endif

// version/domain string
extern char version_domain[];

// Domain metadata is available before the hopping sequence is initialized.
const fhss_config_t &FHSSgetPrimaryConfig();
const fhss_config_t &FHSSgetSecondaryConfig();

// Return the retained count, or zero for invalid metadata. Lookup maps compact
// sequence indexes to original grid indexes; exclusions are processed at startup.
uint32_t FHSSinitialiseBand(const fhss_config_t &config, uint8_t *channelMap,
                           uint_fast8_t &syncChannel, uint32_t &spread);

static inline uint32_t FHSSchannelFrequency(const fhss_config_t &config,
    const uint8_t *channelMap, uint8_t channel, uint32_t spread)
{
    const uint8_t gridChannel = config.exclusion_count ? channelMap[channel] : channel;
    return config.freq_start + spread * gridChannel / FREQ_SPREAD_SCALE;
}

// create and randomise an FHSS sequence
void FHSSrandomiseFHSSsequence(uint32_t seed);
// build one band's sequence from freqCount alone and return the number of
// entries written, always a whole multiple of freqCount
uint16_t FHSSrandomiseFHSSsequenceBuild(uint32_t seed, uint32_t freqCount, uint_fast8_t sync_channel, uint8_t *sequence);

// add domain info for Lua
void addDomainInfo(char *version_domain, uint8_t maxlen);

static inline uint32_t FHSSgetMinimumFreq(void)
{
    return FHSSconfig->freq_start;
}

static inline uint32_t FHSSgetMaximumFreq(void)
{
    return FHSSconfig->freq_stop;
}

// The number of frequencies for this regulatory domain
static inline uint32_t FHSSgetChannelCount(void)
{
    if (FHSSusePrimaryFreqBand)
    {
        return primaryFreqCount;
    }
    else
    {
        return secondaryFreqCount;
    }
}

// how far the shared hop index may advance in the current mode. This is not any
// one band's table length, so never size a sequence build from it
static inline uint16_t FHSSgetSequenceCount()
{
    if (FHSSuseDualBand) // Use the smaller of the 2 bands as not to go beyond the max index for each sequence.
    {
        if (primaryBandCount < secondaryBandCount)
        {
            return primaryBandCount;
        }
        else
        {
            return secondaryBandCount;
        }
    }

    if (FHSSusePrimaryFreqBand)
    {
        return primaryBandCount;
    }
    else
    {
        return secondaryBandCount;
    }
}

// get the initial frequency, which is also the sync channel
static inline uint32_t FHSSgetInitialFreq()
{
    if (FHSSusePrimaryFreqBand)
    {
        return FHSSchannelFrequency(*FHSSconfig, FHSSchannelMap, sync_channel, freq_spread) - FreqCorrection;
    }
    else
    {
        return FHSSchannelFrequency(*FHSSconfigDualBand, FHSSchannelMap_DualBand, sync_channel_DualBand, freq_spread_DualBand);
    }
}

// Get the current sequence pointer
static inline uint8_t FHSSgetCurrIndex()
{
    return FHSSptr;
}

// Is the current frequency the sync frequency
static inline uint8_t FHSSonSyncChannel()
{
    if (FHSSusePrimaryFreqBand)
    {
        return FHSSsequence[FHSSptr] == sync_channel;
    }
    else
    {
        return FHSSsequence_DualBand[FHSSptr] == sync_channel_DualBand;
    }
}

// Set the sequence pointer, used by RX on SYNC
static inline void FHSSsetCurrIndex(const uint8_t value)
{
    FHSSptr = value % FHSSgetSequenceCount();
}

// Advance the pointer to the next hop and return the frequency of that channel
static inline uint32_t FHSSgetNextFreq()
{
    FHSSptr = (FHSSptr + 1) % FHSSgetSequenceCount();

    if (FHSSusePrimaryFreqBand)
    {
        return FHSSchannelFrequency(*FHSSconfig, FHSSchannelMap, FHSSsequence[FHSSptr], freq_spread) - FreqCorrection;
    }
    else
    {
        return FHSSchannelFrequency(*FHSSconfigDualBand, FHSSchannelMap_DualBand, FHSSsequence_DualBand[FHSSptr], freq_spread_DualBand);
    }
}

static inline const char *FHSSgetRegulatoryDomain()
{
    if (FHSSusePrimaryFreqBand)
    {
        return FHSSconfig->domain;
    }
    else
    {
        return FHSSconfigDualBand->domain;
    }
}

// Offset by half of the retained channel list for same-band Gemini
static inline uint32_t FHSSGeminiFreq(uint8_t FHSSsequenceIdx)
{
    uint32_t freq;
    uint32_t numfhss = FHSSgetChannelCount();
    uint8_t offSetIdx = (FHSSsequenceIdx + (numfhss / 2)) % numfhss; 

    if (FHSSusePrimaryFreqBand)
    {
        freq = FHSSchannelFrequency(*FHSSconfig, FHSSchannelMap, offSetIdx, freq_spread) - FreqCorrection_2;
    }
    else
    {
        freq = FHSSchannelFrequency(*FHSSconfigDualBand, FHSSchannelMap_DualBand, offSetIdx, freq_spread_DualBand);
    }

    return freq;
}

static inline uint32_t FHSSgetGeminiFreq()
{
    if (FHSSuseDualBand)
    {
        // When using Dual Band there is no need to calculate an offset frequency. Unlike Gemini with 2 frequencies in the same band.
        return FHSSchannelFrequency(*FHSSconfigDualBand, FHSSchannelMap_DualBand, FHSSsequence_DualBand[FHSSptr], freq_spread_DualBand);
    }
    else
    {
        if (FHSSusePrimaryFreqBand)
        {
            return FHSSGeminiFreq(FHSSsequence[FHSSgetCurrIndex()]);
        }
        else
        {
            return FHSSGeminiFreq(FHSSsequence_DualBand[FHSSgetCurrIndex()]);
        }
    }
}

static inline uint32_t FHSSgetInitialGeminiFreq()
{
    if (FHSSuseDualBand)
    {
        return FHSSchannelFrequency(*FHSSconfigDualBand, FHSSchannelMap_DualBand, sync_channel_DualBand, freq_spread_DualBand);
    }
    else
    {
        if (FHSSusePrimaryFreqBand)
        {
            return FHSSGeminiFreq(sync_channel);
        }
        else
        {
            return FHSSGeminiFreq(sync_channel_DualBand);
        }
    }
}
