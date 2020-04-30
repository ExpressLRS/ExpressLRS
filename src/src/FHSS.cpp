#include "FHSS.h"
#include "debug.h"
#include "LoRaRadioLib.h"
#include "common.h"
#include "utils.h"

// Our table of FHSS frequencies. Define a regulatory domain to select the correct set for your location and radio
#ifdef Regulatory_Domain_AU_433
static uint32_t DRAM_ATTR FHSSfreqs[] = {
    433420000,
    433920000,
    434420000};

#elif defined Regulatory_Domain_AU_915
static uint32_t DRAM_ATTR FHSSfreqs[] = {
    915500000,
    916100000,
    916700000,
    917300000,

    917900000,
    918500000,
    919100000,
    919700000,

    920300000,
    920900000,
    921500000,
    922100000,

    922700000,
    923300000,
    923900000,
    924500000,

    925100000,
    925700000,
    926300000,
    926900000};

#elif defined Regulatory_Domain_EU_868
#define FRSKY_FREQS 0

/* Frequency bands taken from https://wetten.overheid.nl/BWBR0036378/2016-12-28#Bijlagen
 * Note: these frequencies fall in the license free H-band, but in combination with 500kHz
 * LoRa modem bandwidth used by ExpressLRS (EU allows up to 125kHz modulation BW only) they
 * will never pass RED certification and they are ILLEGAL to use.
 *
 * Therefore we simply maximize the usage of available spectrum so laboratory testing of the software won't disturb existing
 * 868MHz ISM band traffic too much.
 */
#if !FRSKY_FREQS
static uint32_t DRAM_ATTR FHSSfreqs[] = {
    863275000, // band H1, 863 - 865MHz, 0.1% duty cycle or CSMA techniques, 25mW EIRP
    863800000,
    864325000,
    864850000,
    865375000, // Band H2, 865 - 868.6MHz, 1.0% dutycycle or CSMA, 25mW EIRP
    865900000,
    866425000,
    866950000,
    867475000,
    868000000,
    868525000, // Band H3, 868.7-869.2MHz, 0.1% dutycycle or CSMA, 25mW EIRP
    869050000,
    869575000};

#else //FRSKY_FREQS
// https://github.com/pascallanger/DIY-Multiprotocol-TX-Module/blob/4ae30dc3b049f18147d6e278817f7a5f425c2fb0/Multiprotocol/FrSkyR9_sx1276.ino#L43
static uint32_t DRAM_ATTR FHSSfreqs[] = {
    // FrSkyR9_freq_map_868
    859504640,
    860004352,
    860504064,
    861003776,
    861503488,
    862003200,
    862502912,
    863002624,
    863502336,
    864002048,
    864501760,
    865001472,
    865501184,
    866000896,
    866500608,
    867000320,
    867500032,
    867999744,
    868499456,
    868999168,
    869498880,
    869998592,
    870498304,
    870998016,
    871497728,
    871997440,
    872497152,
    // last two determined by FrSkyR9_step
    /*0, 0*/
};
#endif

#elif defined Regulatory_Domain_EU_433
/* Frequency band G, taken from https://wetten.overheid.nl/BWBR0036378/2016-12-28#Bijlagen
 * Note: As is the case with the 868Mhz band, these frequencies only comply to the license free portion
 * of the spectrum, nothing else. As such, these are likely illegal to use.
 */
static uint32_t DRAM_ATTR FHSSfreqs[] = {
    433100000,
    433925000,
    434450000};

#elif defined Regulatory_Domain_FCC_915
/* Very definitely not fully checked. An initial pass at increasing the hops
*/
static uint32_t DRAM_ATTR FHSSfreqs[] = {
    903500000,
    904100000,
    904700000,
    905300000,

    905900000,
    906500000,
    907100000,
    907700000,

    908300000,
    908900000,
    909500000,
    910100000,

    910700000,
    911300000,
    911900000,
    912500000,

    913100000,
    913700000,
    914300000,
    914900000,

    915500000, // as per AU..
    916100000,
    916700000,
    917300000,

    917900000,
    918500000,
    919100000,
    919700000,

    920300000,
    920900000,
    921500000,
    922100000,

    922700000,
    923300000,
    923900000,
    924500000,

    925100000,
    925700000,
    926300000,
    926900000};

#else
#error No regulatory domain defined, please define one in common.h
#endif

#define NR_SEQUENCE_ENTRIES 256
uint_fast8_t volatile DRAM_ATTR FHSSptr = 0;
uint8_t DRAM_ATTR FHSSsequence[NR_SEQUENCE_ENTRIES] = {0};

#define FREQ_OFFSET_UID (UID[4] + UID[5])
int_fast32_t volatile DRAM_ATTR FreqCorrection = FREQ_OFFSET_UID;

void ICACHE_RAM_ATTR FHSSresetFreqCorrection()
{
    FreqCorrection = FREQ_OFFSET_UID;
}

void ICACHE_RAM_ATTR FHSSsetCurrIndex(uint8_t value)
{ // set the current index of the FHSS pointer
    FHSSptr = value % sizeof(FHSSsequence);
}

uint8_t ICACHE_RAM_ATTR FHSSgetCurrIndex()
{ // get the current index of the FHSS pointer
    return FHSSptr;
}

void ICACHE_RAM_ATTR FHSSincCurrIndex()
{
    FHSSptr = (FHSSptr + 1) % sizeof(FHSSsequence);
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
static void resetIsAvailable(uint8_t * const array, uint32_t size)
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
#elif defined Regulatory_Domain_EU_868
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
