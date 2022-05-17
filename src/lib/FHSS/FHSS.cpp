#include "FHSS.h"
#include "logging.h"
#include "options.h"
#include <string.h>

#if defined(RADIO_SX127X)
#include "SX127xDriver.h"

const fhss_config_t domains[] = {
    {"AU915",  915500000, 600000, 20},
    {"FCC915", 903500000, 600000, 40},
    {"EU868",  865275000, 525000, 13},
    {"IN866",  865375000, 525000, 4},
    {"AU433",  433420000, 500000, 3},
    {"EU433",  433100000, 675000, 3}
};
#elif defined(RADIO_SX128X)
#include "SX1280Driver.h"

const fhss_config_t domains[] = {
    {"ISM2G4", 2400400000, 1000000, 80}
};
#endif

// Our table of FHSS frequencies. Define a regulatory domain to select the correct set for your location and radio
const fhss_config_t *FHSSconfig;

// Actual sequence of hops as indexes into the frequency list
uint8_t FHSSsequence[256];
// Which entry in the sequence we currently are on
uint8_t volatile FHSSptr;
// Channel for sync packets and initial connection establishment
uint_fast8_t sync_channel;
// Offset from the predefined frequency determined by AFC on Team900 (register units)
int32_t FreqCorrection;

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
void FHSSrandomiseFHSSsequence(const uint32_t seed)
{
    FHSSconfig = &domains[firmwareOptions.domain];
    INFOLN("Setting %s Mode", FHSSconfig->domain);
    DBGLN("Number of FHSS frequencies = %u", FHSSconfig->freq_count);

    sync_channel = FHSSconfig->freq_count / 2;
    DBGLN("Sync channel = %u", sync_channel);

    // reset the pointer (otherwise the tests fail)
    FHSSptr = 0;
    rngSeed(seed);

    // initialize the sequence array
    for (uint8_t i = 0; i < FHSSgetSequenceCount(); i++)
    {
        if (i % FHSSconfig->freq_count == 0) {
            FHSSsequence[i] = sync_channel;
        } else if (i % FHSSconfig->freq_count == sync_channel) {
            FHSSsequence[i] = 0;
        } else {
            FHSSsequence[i] = i % FHSSconfig->freq_count;
        }
    }

    for (uint8_t i=0; i < FHSSgetSequenceCount(); i++)
    {
        // if it's not the sync channel
        if (i % FHSSconfig->freq_count != 0)
        {
            uint8_t offset = (i / FHSSconfig->freq_count) * FHSSconfig->freq_count; // offset to start of current block
            uint8_t rand = rngN(FHSSconfig->freq_count-1)+1; // random number between 1 and FHSS_FREQ_CNT

            // switch this entry and another random entry in the same block
            uint8_t temp = FHSSsequence[i];
            FHSSsequence[i] = FHSSsequence[offset+rand];
            FHSSsequence[offset+rand] = temp;
        }
    }

    // output FHSS sequence
    for (uint8_t i=0; i < FHSSgetSequenceCount(); i++)
    {
        DBG("%u ",FHSSsequence[i]);
        if (i % 10 == 9)
            DBGCR;
    }
    DBGCR;
}

bool isDomain868()
{
    return strcmp(FHSSconfig->domain, "EU868") == 0;
}