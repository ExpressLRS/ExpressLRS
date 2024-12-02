#include "FHSS.h"
#include "logging.h"
#include "options.h"
#include <string.h>

#if defined(RADIO_SX127X) || defined(RADIO_LR1121)

#if defined(RADIO_LR1121)
#include "LR1121Driver.h"
#else
#include "SX127xDriver.h"
#endif

const fhss_config_t domains[] = {
    {"W970",  FREQ_HZ_TO_REG_VAL(970375000), FREQ_HZ_TO_REG_VAL(971950000), 4, 971000000},
    {"W760",  FREQ_HZ_TO_REG_VAL(760375000), FREQ_HZ_TO_REG_VAL(761950000), 4, 761000000},
    {"W770",  FREQ_HZ_TO_REG_VAL(770375000), FREQ_HZ_TO_REG_VAL(771950000), 4, 771000000},
    {"W865",  FREQ_HZ_TO_REG_VAL(865375000), FREQ_HZ_TO_REG_VAL(866950000), 4, 866000000},
    {"W780",  FREQ_HZ_TO_REG_VAL(780375000), FREQ_HZ_TO_REG_VAL(781950000), 4, 781000000},
    {"W790",  FREQ_HZ_TO_REG_VAL(790375000), FREQ_HZ_TO_REG_VAL(791950000), 4, 791000000},
    {"W800",  FREQ_HZ_TO_REG_VAL(800375000), FREQ_HZ_TO_REG_VAL(801950000), 4, 801000000},
    {"W810",  FREQ_HZ_TO_REG_VAL(810375000), FREQ_HZ_TO_REG_VAL(811950000), 4, 811000000},
    {"W820",  FREQ_HZ_TO_REG_VAL(820375000), FREQ_HZ_TO_REG_VAL(821950000), 4, 821000000},
    {"W830",  FREQ_HZ_TO_REG_VAL(830375000), FREQ_HZ_TO_REG_VAL(831950000), 4, 831000000},
    {"W840",  FREQ_HZ_TO_REG_VAL(840375000), FREQ_HZ_TO_REG_VAL(841950000), 4, 841000000},
    {"W850",  FREQ_HZ_TO_REG_VAL(850375000), FREQ_HZ_TO_REG_VAL(851950000), 4, 851000000},
    {"W860",  FREQ_HZ_TO_REG_VAL(860375000), FREQ_HZ_TO_REG_VAL(861950000), 4, 861000000},
    {"W870",  FREQ_HZ_TO_REG_VAL(870375000), FREQ_HZ_TO_REG_VAL(871950000), 4, 871000000},
    {"W880",  FREQ_HZ_TO_REG_VAL(880375000), FREQ_HZ_TO_REG_VAL(881950000), 4, 881000000},
    {"W890",  FREQ_HZ_TO_REG_VAL(890375000), FREQ_HZ_TO_REG_VAL(891950000), 4, 891000000},
    {"W900",  FREQ_HZ_TO_REG_VAL(900375000), FREQ_HZ_TO_REG_VAL(901950000), 4, 901000000},
    {"W910",  FREQ_HZ_TO_REG_VAL(910375000), FREQ_HZ_TO_REG_VAL(911950000), 4, 911000000},
    {"W920",  FREQ_HZ_TO_REG_VAL(920375000), FREQ_HZ_TO_REG_VAL(921950000), 4, 921000000},
    {"W930",  FREQ_HZ_TO_REG_VAL(930375000), FREQ_HZ_TO_REG_VAL(931950000), 4, 931000000},
    {"W940",  FREQ_HZ_TO_REG_VAL(940375000), FREQ_HZ_TO_REG_VAL(941950000), 4, 941000000},
    {"W950",  FREQ_HZ_TO_REG_VAL(950375000), FREQ_HZ_TO_REG_VAL(951950000), 4, 951000000},
    {"W960",  FREQ_HZ_TO_REG_VAL(960375000), FREQ_HZ_TO_REG_VAL(961950000), 4, 961000000},
    {"W980",  FREQ_HZ_TO_REG_VAL(980375000), FREQ_HZ_TO_REG_VAL(981950000), 4, 981000000}
};

#if defined(RADIO_LR1121)
const fhss_config_t domainsDualBand[] = {
    {"W2250", FREQ_HZ_TO_REG_VAL(2250400000), FREQ_HZ_TO_REG_VAL(2252400000), 8, 2251000000},
    {"W760",  FREQ_HZ_TO_REG_VAL(760375000), FREQ_HZ_TO_REG_VAL(761950000), 4, 761000000},
    {"W770",  FREQ_HZ_TO_REG_VAL(770375000), FREQ_HZ_TO_REG_VAL(771950000), 4, 771000000},
    {"W865",  FREQ_HZ_TO_REG_VAL(865375000), FREQ_HZ_TO_REG_VAL(866950000), 4, 866000000},
    {"W780",  FREQ_HZ_TO_REG_VAL(780375000), FREQ_HZ_TO_REG_VAL(781950000), 4, 781000000},
    {"W790",  FREQ_HZ_TO_REG_VAL(790375000), FREQ_HZ_TO_REG_VAL(791950000), 4, 791000000},
    {"W800",  FREQ_HZ_TO_REG_VAL(800375000), FREQ_HZ_TO_REG_VAL(801950000), 4, 801000000},
    {"W810",  FREQ_HZ_TO_REG_VAL(810375000), FREQ_HZ_TO_REG_VAL(811950000), 4, 811000000},
    {"W820",  FREQ_HZ_TO_REG_VAL(820375000), FREQ_HZ_TO_REG_VAL(821950000), 4, 821000000},
    {"W830",  FREQ_HZ_TO_REG_VAL(830375000), FREQ_HZ_TO_REG_VAL(831950000), 4, 831000000},
    {"W840",  FREQ_HZ_TO_REG_VAL(840375000), FREQ_HZ_TO_REG_VAL(841950000), 4, 841000000},
    {"W850",  FREQ_HZ_TO_REG_VAL(850375000), FREQ_HZ_TO_REG_VAL(851950000), 4, 851000000},
    {"W860",  FREQ_HZ_TO_REG_VAL(860375000), FREQ_HZ_TO_REG_VAL(861950000), 4, 861000000},
    {"W870",  FREQ_HZ_TO_REG_VAL(870375000), FREQ_HZ_TO_REG_VAL(871950000), 4, 871000000},
    {"W880",  FREQ_HZ_TO_REG_VAL(880375000), FREQ_HZ_TO_REG_VAL(881950000), 4, 881000000},
    {"W890",  FREQ_HZ_TO_REG_VAL(890375000), FREQ_HZ_TO_REG_VAL(891950000), 4, 891000000},
    {"W900",  FREQ_HZ_TO_REG_VAL(900375000), FREQ_HZ_TO_REG_VAL(901950000), 4, 901000000},
    {"W910",  FREQ_HZ_TO_REG_VAL(910375000), FREQ_HZ_TO_REG_VAL(911950000), 4, 911000000},
    {"W920",  FREQ_HZ_TO_REG_VAL(920375000), FREQ_HZ_TO_REG_VAL(921950000), 4, 921000000},
    {"W930",  FREQ_HZ_TO_REG_VAL(930375000), FREQ_HZ_TO_REG_VAL(931950000), 4, 931000000},
    {"W940",  FREQ_HZ_TO_REG_VAL(940375000), FREQ_HZ_TO_REG_VAL(941950000), 4, 941000000},
    {"W950",  FREQ_HZ_TO_REG_VAL(950375000), FREQ_HZ_TO_REG_VAL(951950000), 4, 951000000},
    {"W960",  FREQ_HZ_TO_REG_VAL(960375000), FREQ_HZ_TO_REG_VAL(961950000), 4, 961000000},
    {"W970",  FREQ_HZ_TO_REG_VAL(970375000), FREQ_HZ_TO_REG_VAL(971950000), 4, 971000000},
    {"W980",  FREQ_HZ_TO_REG_VAL(980375000), FREQ_HZ_TO_REG_VAL(981950000), 4, 981000000},
    {"W2100", FREQ_HZ_TO_REG_VAL(2100400000), FREQ_HZ_TO_REG_VAL(2102400000), 8, 2101000000},
    {"W2150", FREQ_HZ_TO_REG_VAL(2150400000), FREQ_HZ_TO_REG_VAL(2152400000), 8, 2151000000},
    {"W2200", FREQ_HZ_TO_REG_VAL(2200400000), FREQ_HZ_TO_REG_VAL(2202400000), 8, 2201000000},
    {"W2300", FREQ_HZ_TO_REG_VAL(2300400000), FREQ_HZ_TO_REG_VAL(2302400000), 8, 2301000000},
    {"W2350", FREQ_HZ_TO_REG_VAL(2350400000), FREQ_HZ_TO_REG_VAL(2352400000), 8, 2351000000},
    {"W2400", FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2402400000), 8, 2401000000},
    {"W2450", FREQ_HZ_TO_REG_VAL(2450400000), FREQ_HZ_TO_REG_VAL(2452400000), 8, 2451000000},
    {"W2500", FREQ_HZ_TO_REG_VAL(2500400000), FREQ_HZ_TO_REG_VAL(2502400000), 8, 2501000000},
    {"W2550", FREQ_HZ_TO_REG_VAL(2550400000), FREQ_HZ_TO_REG_VAL(2552400000), 8, 2551000000},
    {"W2600", FREQ_HZ_TO_REG_VAL(2600400000), FREQ_HZ_TO_REG_VAL(2602400000), 8, 2601000000},
    {"W2650", FREQ_HZ_TO_REG_VAL(2650400000), FREQ_HZ_TO_REG_VAL(2652400000), 8, 2651000000},
    {"W2700", FREQ_HZ_TO_REG_VAL(2700400000), FREQ_HZ_TO_REG_VAL(2702400000), 8, 2701000000}
};
#endif

#elif defined(RADIO_SX128X)
#include "SX1280Driver.h"

const fhss_config_t domains[] = {
    {"W2250", FREQ_HZ_TO_REG_VAL(2250400000), FREQ_HZ_TO_REG_VAL(2252400000), 8, 2251000000},
    {"W2100", FREQ_HZ_TO_REG_VAL(2100400000), FREQ_HZ_TO_REG_VAL(2102400000), 8, 2101000000},
    {"W2150", FREQ_HZ_TO_REG_VAL(2150400000), FREQ_HZ_TO_REG_VAL(2152400000), 8, 2151000000},
    {"W2200", FREQ_HZ_TO_REG_VAL(2200400000), FREQ_HZ_TO_REG_VAL(2202400000), 8, 2201000000},
    {"W2300", FREQ_HZ_TO_REG_VAL(2300400000), FREQ_HZ_TO_REG_VAL(2302400000), 8, 2301000000},
    {"W2350", FREQ_HZ_TO_REG_VAL(2350400000), FREQ_HZ_TO_REG_VAL(2352400000), 8, 2351000000},
    {"W2400", FREQ_HZ_TO_REG_VAL(2400400000), FREQ_HZ_TO_REG_VAL(2402400000), 8, 2401000000},
    {"W2450", FREQ_HZ_TO_REG_VAL(2450400000), FREQ_HZ_TO_REG_VAL(2452400000), 8, 2451000000},
    {"W2500", FREQ_HZ_TO_REG_VAL(2500400000), FREQ_HZ_TO_REG_VAL(2502400000), 8, 2501000000},
    {"W2550", FREQ_HZ_TO_REG_VAL(2550400000), FREQ_HZ_TO_REG_VAL(2552400000), 8, 2551000000},
    {"W2600", FREQ_HZ_TO_REG_VAL(2600400000), FREQ_HZ_TO_REG_VAL(2602400000), 8, 2601000000},
    {"W2650", FREQ_HZ_TO_REG_VAL(2650400000), FREQ_HZ_TO_REG_VAL(2652400000), 8, 2651000000},
    {"W2700", FREQ_HZ_TO_REG_VAL(2700400000), FREQ_HZ_TO_REG_VAL(2702400000), 8, 2701000000}
};
#endif

// Our table of FHSS frequencies. Define a regulatory domain to select the correct set for your location and radio
const fhss_config_t *FHSSconfig;
const fhss_config_t *FHSSconfigDualBand;

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
bool FHSSuseDualBand = true;

uint16_t primaryBandCount;
uint16_t secondaryBandCount;

void FHSSrandomiseFHSSsequence(const uint32_t seed)
{
    FHSSconfig = &domains[firmwareOptions.domain0];
    sync_channel = (FHSSconfig->freq_count / 2) + 1;
    freq_spread = (FHSSconfig->freq_stop - FHSSconfig->freq_start) * FREQ_SPREAD_SCALE / (FHSSconfig->freq_count - 1);
    primaryBandCount = (FHSS_SEQUENCE_LEN / FHSSconfig->freq_count) * FHSSconfig->freq_count;

    DBGLN("Setting %s Mode", FHSSconfig->domain);
    DBGLN("Number of FHSS frequencies = %u", FHSSconfig->freq_count);
    DBGLN("Sync channel = %u", sync_channel);

    FHSSrandomiseFHSSsequenceBuild(seed, FHSSconfig->freq_count, sync_channel, FHSSsequence);

#if defined(RADIO_LR1121)
    FHSSconfigDualBand = &domainsDualBand[firmwareOptions.domain1];
    sync_channel_DualBand = (FHSSconfigDualBand->freq_count / 2) + 1;
    freq_spread_DualBand = (FHSSconfigDualBand->freq_stop - FHSSconfigDualBand->freq_start) * FREQ_SPREAD_SCALE / (FHSSconfigDualBand->freq_count - 1);
    secondaryBandCount = (FHSS_SEQUENCE_LEN / FHSSconfigDualBand->freq_count) * FHSSconfigDualBand->freq_count;

    DBGLN("Setting Dual Band %s Mode", FHSSconfigDualBand->domain);
    DBGLN("Number of FHSS frequencies = %u", FHSSconfigDualBand->freq_count);
    DBGLN("Sync channel Dual Band = %u", sync_channel_DualBand);

    FHSSusePrimaryFreqBand = false;
    FHSSrandomiseFHSSsequenceBuild(seed, FHSSconfigDualBand->freq_count, sync_channel_DualBand, FHSSsequence_DualBand);
    FHSSusePrimaryFreqBand = true;
#endif
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
    return strcmp(FHSSconfig->domain, "EU868") == 0;
}
