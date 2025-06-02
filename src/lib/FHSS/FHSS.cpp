#include "FHSS.h"
#include "logging.h"
#include "options.h"
#include <string.h>
#include "SX127xDriver.h"
#include "crsf_protocol.h"

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

static toggleTxCmd_t toggle_tx_cmd;
static fhss_config_t N_FHSSconfig;

uint32_t freqHzToRegVal(double freq) {
    return static_cast<uint32_t>(freq /FREQ_STEP);
}

double freqRegValToMHz(uint32_t reg_val) {
    return (static_cast<double>(reg_val + FreqCorrection) * FREQ_STEP) / 1000000.0;
}

#ifdef TX_BAND_HIGH
    uint16_t startBase   = ISM_HIGH_BAND_FREQ_START;
    uint16_t endBase     = ISM_HIGH_BAND_FREQ_END;
    uint8_t  numChannels = TOTAL_CHANNELS_20;
#elif TX_BAND_LOW
    uint16_t startBase  = LOW_BAND_FREQ_START;
    uint16_t endBase    = LOW_BAND_FREQ_END;
    uint8_t numChannels = TOTAL_CHANNELS_20;
#else /* default to LOW band */
    uint16_t startBase  = LOW_BAND_FREQ_START;
    uint16_t endBase    = LOW_BAND_FREQ_END;
    uint8_t numChannels = TOTAL_CHANNELS_20;
#endif

uint32_t startFrequency = freqHzToRegVal(startBase*100000);
uint32_t endFrequency   = freqHzToRegVal(endBase*100000);
uint32_t midFrequency = 970000000;//915000000;

static void setupFHSS(fhss_config_t *config)
{
    config->freq_start = freqHzToRegVal(startBase*100000);
    config->freq_stop = freqHzToRegVal(endBase*100000);
    config->num_channels = numChannels;

    if(startBase == ISM_HIGH_BAND_FREQ_START && endBase == ISM_HIGH_BAND_FREQ_END)
    {
        config->domain = "ISM_915";
        config->active_band = ISM_915;
    }
    else if(startBase == EW_HIGH_BAND_FREQ_START && endBase == EW_HIGH_BAND_FREQ_END)
    {
        config->domain = "EW_970";
        config->active_band = EW_970;
    }

    config->is_band_changing = false;
}

fhss_config_t *getFHSSconfig()
{
    return &N_FHSSconfig;
}

void updateHighBandChannel(uint8_t *bufferIn)
{
    fhss_config_t *config = getFHSSconfig();

    memcpy(&toggle_tx_cmd, bufferIn, sizeof(toggle_tx_cmd));

    if(toggle_tx_cmd.active_highband == ISM_915)
    {
        startBase   = ISM_HIGH_BAND_FREQ_START;
        endBase     = ISM_HIGH_BAND_FREQ_END;
        numChannels = TOTAL_CHANNELS_20;
        config->active_band = ISM_915;
        config->domain = "ISM_915";
    }
    else if(toggle_tx_cmd.active_highband == EW_970)
    {
        startBase   = EW_HIGH_BAND_FREQ_START;
        endBase     = EW_HIGH_BAND_FREQ_END;
        numChannels = TOTAL_CHANNELS_40;
        config->active_band = EW_970;
        config->domain = "EW_970";
    }

    config->freq_start = freqHzToRegVal(startBase * 100000);
    config->freq_stop  = freqHzToRegVal(endBase * 100000);
    config->num_channels = numChannels;
    config->is_band_changing = true; // a change is coming
}

void FHSSrandomiseFHSSsequence(const uint32_t seed)
{
    fhss_config_t *config = getFHSSconfig();
    static bool setupDone = false;
    if(!setupDone)
    {
        setupFHSS(config);
        setupDone = true;
    }

    sync_channel = (config->num_channels / 2) + 1;
    freq_spread  = (config->freq_stop - config->freq_start) * FREQ_SPREAD_SCALE / (config->num_channels - 1);
    primaryBandCount = (FHSS_SEQUENCE_LEN / config->num_channels) * config->num_channels;

    FHSSrandomiseFHSSsequenceBuild(seed, config->num_channels, sync_channel, FHSSsequence);
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