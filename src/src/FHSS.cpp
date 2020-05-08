#include "FHSS.h"
#include "debug.h"
#include "LoRaRadioLib.h"
#include "common.h"
#include "utils.h"

#include "fhss_freqs.h"

#ifndef FHSS_MY_STEP
#define FHSS_MY_STEP 1
#endif

#if NR_SEQUENCE_ENTRIES <= 256
uint_fast8_t volatile DRAM_ATTR FHSSptr = 0;
#elif NR_SEQUENCE_ENTRIES <= 65536
uint_fast16_t volatile DRAM_ATTR FHSSptr = 0;
#else
#error "FHSS sequence len invalid!"
#endif
int_fast32_t volatile DRAM_ATTR FreqCorrection = 0;

void ICACHE_RAM_ATTR FHSSresetFreqCorrection()
{
    FreqCorrection = 0;
}

void ICACHE_RAM_ATTR FHSSsetCurrIndex(uint32_t value)
{ // set the current index of the FHSS pointer
    FHSSptr = value % sizeof(FHSSsequence);
}

uint32_t ICACHE_RAM_ATTR FHSSgetCurrIndex()
{ // get the current index of the FHSS pointer
    return FHSSptr;
}

void ICACHE_RAM_ATTR FHSSincCurrIndex()
{
    FHSSptr = (FHSSptr + FHSS_MY_STEP) % sizeof(FHSSsequence);
}

uint8_t ICACHE_RAM_ATTR FHSSgetCurrSequenceIndex()
{
    return FHSSsequence[FHSSptr];
}

uint32_t ICACHE_RAM_ATTR GetInitialFreq()
{
    return FHSSfreqs[0] - FreqCorrection;
}

uint32_t ICACHE_RAM_ATTR FHSSgetCurrFreq()
{
    return FHSSfreqs[FHSSsequence[FHSSptr]] - FreqCorrection;
}

uint32_t ICACHE_RAM_ATTR FHSSgetNextFreq()
{
    FHSSincCurrIndex();
    return FHSSgetCurrFreq();
}

// Set all of the flags in the array to true, except for the first one
// which corresponds to the sync channel and is never available for normal
// allocation.
static void resetIsAvailable(uint8_t *const array, uint32_t size)
{
    // channel 0 is the sync channel and is never considered available
    array[0] = 0;

    // all other entires to 1
    for (uint32_t i = 1; i < size; i++)
        array[i] = 1;
}

/**
Requirements:
1. 0 every n hops
2. No two repeated channels
3. Equal occurance of each (or as even as possible) of each channel
4. Pesudorandom

Approach:
  Initialise an array of flags indicating which channels have not yet been assigned and a counter of how many channels are available
  Iterate over the FHSSsequence array using index
    if index is a multiple of SYNC_INTERVAL assign the sync channel index (0)
    otherwise, generate a random number between 0 and the number of channels left to be assigned
    find the index of the nth remaining channel
    if the index is a repeat, generate a new random number
    if the index is not a repeat, assing it to the FHSSsequence array, clear the availability flag and decrement the available count
    if there are no available channels left, reset the flags array and the count
*/
void FHSSrandomiseFHSSsequence()
{
    const uint32_t NR_FHSS_ENTRIES = (sizeof(FHSSfreqs) / sizeof(FHSSfreqs[0]));

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_FCC_915)
    DEBUG_PRINTLN("Setting 915MHz Mode");
#elif defined Regulatory_Domain_EU_868 || defined Regulatory_Domain_EU_868_R9
    DEBUG_PRINTLN("Setting 868MHz Mode");
#elif defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
    DEBUG_PRINTLN("Setting 433MHz Mode");
#else
#error No regulatory domain defined, please define one in common.h
#endif

    DEBUG_PRINT("Number of FHSS frequencies =");
    DEBUG_PRINTLN(NR_FHSS_ENTRIES);

    uint32_t macSeed = ((uint32_t)UID[2] << 24) +
                       ((uint32_t)UID[3] << 16) +
                       ((uint32_t)UID[4] << 8) +
                       UID[5];
    rngSeed(macSeed);

    uint8_t isAvailable[NR_FHSS_ENTRIES];

    resetIsAvailable(isAvailable, sizeof(isAvailable));

    // Fill the FHSSsequence with channel indices
    // The 0 index is special - the 'sync' channel. The sync channel appears every
    // syncInterval hops. The other channels are randomly distributed between the
    // sync channels
    const int SYNC_INTERVAL = 20;

    int nLeft = NR_FHSS_ENTRIES - 1; // how many channels are left to be allocated. Does not include the sync channel
    unsigned int prev = 0;           // needed to prevent repeats of the same index

    // for each slot in the sequence table
    for (uint32_t i = 0; i < sizeof(FHSSsequence); i++)
    {
        if (i % SYNC_INTERVAL == 0)
        {
            // assign sync channel 0
            FHSSsequence[i] = 0;
            prev = 0;
        }
        else
        {
            // pick one of the available channels. May need to loop to avoid repeats
            unsigned int index;
            do
            {
                int c = rngN(nLeft); // returnc 0<c<nLeft
                // find the c'th entry in the isAvailable array
                // can skip 0 as that's the sync channel and is never available for normal allocation
                index = 1;
                int found = 0;
                while (index < NR_FHSS_ENTRIES)
                {
                    if (isAvailable[index])
                    {
                        if (found == c)
                            break;
                        found++;
                    }
                    index++;
                }
                if (index == NR_FHSS_ENTRIES)
                {
                    // This should never happen
                    DEBUG_PRINT("FAILED to find the available entry!\n");
                    // What to do? We don't want to hang as that will stop us getting to the wifi hotspot
                    // Use the sync channel
                    index = 0;
                    break;
                }
            } while (index == prev); // can't use index if it repeats the previous value

            FHSSsequence[i] = index; // assign the value to the sequence array
            isAvailable[index] = 0;  // clear the flag
            prev = index;            // remember for next iteration
            nLeft--;                 // reduce the count of available channels
            if (nLeft == 0)
            {
                // we've assigned all of the channels, so reset for next cycle
                resetIsAvailable(isAvailable, sizeof(isAvailable));
                nLeft = NR_FHSS_ENTRIES - 1;
            }
        }

        DEBUG_PRINT(FHSSsequence[i]);
        if ((i + 1) % 10 == 0)
        {
            DEBUG_PRINTLN();
        }
        else
        {
            DEBUG_PRINT(" ");
        }
    } // for each element in FHSSsequence

    DEBUG_PRINTLN();
}

/** Previous version of FHSSrandomiseFHSSsequence

void FHSSrandomiseFHSSsequence()
{
    DEBUG_PRINT("Number of FHSS frequencies =");
    DEBUG_PRINT(NR_FHSS_ENTRIES);

    long macSeed = ((long)UID[2] << 24) + ((long)UID[3] << 16) + ((long)UID[4] << 8) + UID[5];
    rngSeed(macSeed);

    const int hopSeqLength = 256;
    const int numOfFreqs = NR_FHSS_ENTRIES-1;
    const int limit = floor(hopSeqLength / numOfFreqs);

    DEBUG_PRINT("limit =");
    DEBUG_PRINT(limit);

    DEBUG_PRINT("FHSSsequence[] = ");

    int prev_val = 0;
    int rand = 0;

    int last_InitialFreq = 0;
    const int last_InitialFreq_interval = numOfFreqs;

    int tracker[NR_FHSS_ENTRIES] = {0};

    for (int i = 0; i < hopSeqLength; i++)
    {

        if (i >= (last_InitialFreq + last_InitialFreq_interval))
        {
            rand = 0;
            last_InitialFreq = i;
        }
        else
        {
            while (prev_val == rand || rand > numOfFreqs || tracker[rand] >= limit || rand == 0)
            {
                rand = rng5Bit();
            }
        }

        FHSSsequence[i] = rand;
        tracker[rand]++;
        prev_val = rand;

        DEBUG_PRINT(FHSSsequence[i]);
        DEBUG_PRINT(", ");
    }

// Note DaBit: is it really necessary that this is different logic? FHSSsequence[0] is never 0, and it is just a starting frequency anyway?

    // int prev_val = rng0to2(); // Randomised so that FHSSsequence[0] can also be 0.
    // int rand = 0;

    // for (int i = 0; i < 256; i++)
    // {
    //     while (prev_val == rand)
    //     {
    //         rand = rng0to2();
    //     }

    //     prev_val = rand;
    //     FHSSsequence[i] = rand;

    //     DEBUG_PRINT(FHSSsequence[i]);
    //     DEBUG_PRINT(", ");
    // }


    DEBUG_PRINTLN("");
}
*/
