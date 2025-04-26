#include "FHSS.h"
#include "logging.h"
#include "options.h"
#include <string.h>
#include "SX127xDriver.h"

/* ISM High Band */
static constexpr uint16_t ISM_HIGH_BAND_FREQ_START =  9200;
static constexpr uint16_t ISM_HIGH_BAND_FREQ_END   = 10200;
/* STD High Band */
static constexpr uint16_t STD_HIGH_BAND_FREQ_START = 9035;
static constexpr uint16_t STD_HIGH_BAND_FREQ_END   = 9269;
/* LOW High Band */
static constexpr uint16_t LOW_BAND_FREQ_START = 7850;
static constexpr uint16_t LOW_BAND_FREQ_END   = 8350;
/* Total Possible Channels */
static constexpr uint8_t  TOTAL_CHANNELS_20 = 20;
static constexpr uint8_t  TOTAL_CHANNELS_40 = 40;

// Actual sequence of hops as indexes into the frequency list
uint8_t FHSSsequence[FHSS_SEQUENCE_LEN];
uint8_t FHSSsequence_DualBand[FHSS_SEQUENCE_LEN];

// Which entry in the sequence we currently are on
uint8_t volatile FHSSptr;

// Channel for sync packets and initial connection establishment
uint_fast8_t sync_channel;
uint_fast8_t sync_channel_DualBand;

// Offset from the predefined frequency determined by AFC on Team900 (register units)
int32_t FreqCorrection;
int32_t FreqCorrection_2;

// Frequency hop separation
uint32_t freq_spread;
uint32_t freq_spread_DualBand;

// Variable for Dual Band radios
bool FHSSusePrimaryFreqBand = true;
bool FHSSuseDualBand = false;

uint16_t primaryBandCount;
uint16_t secondaryBandCount;

uint32_t freqHzToRegVal(double freq) {
    return static_cast<uint32_t>(freq /FREQ_STEP);
}

double freqRegValToMHz(uint32_t reg_val) {
    return (static_cast<double>(reg_val + FreqCorrection) * FREQ_STEP) / 1000000.0;
}

#ifdef TX_BAND_HIGH
    /* ISM HIGH BAND */
    uint16_t startBase  = ISM_HIGH_BAND_FREQ_START;
    uint16_t endBase    = ISM_HIGH_BAND_FREQ_END;
    uint8_t numChannels = TOTAL_CHANNELS_20;

    // TODO: Will fix with new logic to switch HI between both bands
    /* STD HIGH BAND */
    // uint16_t startBase  = STD_HIGH_BAND_FREQ_START;
    // uint16_t endBase    = STD_HIGH_BAND_FREQ_END;
    // uint8_t numChannels = TOTAL_CHANNELS_40;
#elif TX_BAND_LOW
    uint16_t startBase  = LOW_BAND_FREQ_START;
    uint16_t endBase    = LOW_BAND_FREQ_END;
    uint8_t numChannels = TOTAL_CHANNELS_20;
#else /* default to LOW band */
    uint16_t startBase  = LOW_BAND_FREQ_START;
    uint16_t endBase    = LOW_BAND_FREQ_END;
    uint8_t numChannels = TOTAL_CHANNELS_20;
#endif

uint32_t startFrequency=freqHzToRegVal(startBase*100000);
uint32_t endFrequency=freqHzToRegVal(endBase*100000);
uint32_t midFrequency=915000000;


void FHSSrandomiseFHSSsequence(const uint32_t seed)
{
    sync_channel = (numChannels / 2) + 1;
    freq_spread = (endFrequency- startFrequency) * FREQ_SPREAD_SCALE / (numChannels - 1);
    primaryBandCount = (FHSS_SEQUENCE_LEN / numChannels) * numChannels;

    FHSSrandomiseFHSSsequenceBuild(seed, numChannels, sync_channel, FHSSsequence);
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
void FHSSrandomiseFHSSsequenceBuild(const uint32_t seed, uint32_t freqCount, uint_fast8_t syncChannel, uint8_t *inSequence)
{
    // reset the pointer (otherwise the tests fail)
    FHSSptr = 0;
    rngSeed(seed);

    // initialize the sequence array
    for (uint16_t i = 0; i < FHSSgetSequenceCount(); i++)
    {
        if (i % freqCount == 0) {
            inSequence[i] = syncChannel;
        } else if (i % freqCount == syncChannel) {
            inSequence[i] = 0;
        } else {
            inSequence[i] = i % freqCount;
        }
    }

    for (uint16_t i = 0; i < FHSSgetSequenceCount(); i++)
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
    for (uint16_t i=0; i < FHSSgetSequenceCount(); i++)
    {
        DBG("%u ",inSequence[i]);
        if (i % 10 == 9)
            DBGCR;
    }
    DBGCR;
}

bool isDomain868()
{
    return false;
}
