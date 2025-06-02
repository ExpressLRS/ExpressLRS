#pragma once

#include "targets.h"
#include "random.h"
#include "crsf_protocol.h"

#if defined(RADIO_SX127X)
#define FreqCorrectionMax ((int32_t)(100000/FREQ_STEP))
#elif defined(RADIO_LR1121)
#define FreqCorrectionMax ((int32_t)(100000/FREQ_STEP)) // TODO - This needs checking !!!
#elif defined(RADIO_SX128X)
#define FreqCorrectionMax ((int32_t)(200000/FREQ_STEP))
#endif
#define FreqCorrectionMin (-FreqCorrectionMax)

#if defined(RADIO_LR1121)
#define FREQ_HZ_TO_REG_VAL(freq) (freq)
#define FREQ_SPREAD_SCALE 1
#else

#define FREQ_SPREAD_SCALE 256
#endif

#define FHSS_SEQUENCE_LEN 256

/* EW High Band (970) */
constexpr uint16_t EW_HIGH_BAND_FREQ_START =  9200;
constexpr uint16_t EW_HIGH_BAND_FREQ_END   = 10200;
/* ISM High Band (915) */
constexpr uint16_t ISM_HIGH_BAND_FREQ_START = 9035;
constexpr uint16_t ISM_HIGH_BAND_FREQ_END   = 9269;
/* LOW High Band (810) */
constexpr uint16_t LOW_BAND_FREQ_START = 7850;
constexpr uint16_t LOW_BAND_FREQ_END   = 8350;
/* Total Possible Channels */
constexpr uint8_t  TOTAL_CHANNELS_20 = 20;
constexpr uint8_t  TOTAL_CHANNELS_40 = 40;

typedef struct {
    const char  *domain;
    uint32_t    freq_start;
    uint32_t    freq_stop;
    uint32_t    freq_count;
    uint32_t    freq_center;
    uint8_t     num_channels;
    HighBands   active_band;
    bool        is_band_changing;
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

// DualBand Variables
extern bool FHSSusePrimaryFreqBand;
extern bool FHSSuseDualBand;
extern uint16_t secondaryBandCount;
extern uint32_t freq_spread_DualBand;
extern uint8_t FHSSsequence_DualBand[];
extern uint_fast8_t sync_channel_DualBand;
extern const fhss_config_t *FHSSconfigDualBand;

extern uint16_t startBase;
extern uint16_t endBase;
extern uint32_t startFrequency;
extern uint32_t midFrequency;
extern uint32_t endFrequency;
extern uint8_t numChannels;

// create and randomise an FHSS sequence
void FHSSrandomiseFHSSsequence(uint32_t seed);
void FHSSrandomiseFHSSsequenceBuild(uint32_t seed, uint32_t freqCount, uint_fast8_t sync_channel, uint8_t *sequence);

/**
 * @brief gets a pointer to the N_FHSSconfig.
 * @return fhss_config_t* to the N_FHSSconfig.
 */
fhss_config_t *getFHSSconfig();

uint32_t freqHzToRegVal(double freq);
double freqRegValToMHz(uint32_t reg_val);

static inline uint32_t FHSSgetMinimumFreq(void)
{
    return getFHSSconfig()->freq_start;
}

static inline uint32_t FHSSgetMaximumFreq(void)
{
    return getFHSSconfig()->freq_stop;
}

// The number of frequencies for this regulatory domain
static inline uint32_t FHSSgetChannelCount(void)
{
    return getFHSSconfig()->num_channels;
}

// get the number of entries in the FHSS sequence
static inline uint16_t FHSSgetSequenceCount()
{
    return primaryBandCount;
}

// get the initial frequency, which is also the sync channel
static inline uint32_t FHSSgetInitialFreq()
{
    // return startFrequency + (sync_channel * freq_spread / FREQ_SPREAD_SCALE) - FreqCorrection;
    return getFHSSconfig()->freq_start + (sync_channel * freq_spread / FREQ_SPREAD_SCALE) - FreqCorrection;

}

// Get the current sequence pointer
static inline uint8_t FHSSgetCurrIndex()
{
    return FHSSptr;
}

// Is the current frequency the sync frequency
static inline uint8_t FHSSonSyncChannel()
{
    return FHSSsequence[FHSSptr] == sync_channel;
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
    return getFHSSconfig()->freq_start + (freq_spread * FHSSsequence[FHSSptr] / FREQ_SPREAD_SCALE) - FreqCorrection;
}

static inline const char *FHSSgetRegulatoryDomain()
{
    return "TRAMVAY";
}

// Get frequency offset by half of the domain frequency range
static inline uint32_t FHSSGeminiFreq(uint8_t FHSSsequenceIdx)
{
    uint32_t freq;
    uint32_t numfhss = FHSSgetChannelCount();
    uint8_t offSetIdx = (FHSSsequenceIdx + (numfhss / 2)) % numfhss;


    freq = getFHSSconfig()->freq_start + (freq_spread * offSetIdx / FREQ_SPREAD_SCALE) - FreqCorrection_2;

    return freq;
}

static inline uint32_t FHSSgetGeminiFreq()
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

static inline uint32_t FHSSgetInitialGeminiFreq()
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

void updateHighBandChannel(uint8_t *bufferIn);