#include "FHSS.h"

uint8_t volatile FHSSptr = 0;
uint8_t FHSSsequence[256] = {0};

//uint8_t NumOfFHSSfrequencies = 20;
int32_t FreqCorrection = 0;

// Set all of the flags in the array to true, except for the first one
// which corresponds to the sync channel and is never available for normal
// allocation.
void resetIsAvailable(uint8_t *array)
{
    // channel 0 is the sync channel and is never considered available
    array[0] = 0;

    // all other entires to 1
    for (unsigned int i = 1; i < NR_FHSS_ENTRIES; i++)
        array[i] = 1;
}

/**
Requirements:
1. 0 every n hops
2. No two repeated channels
3. Equal occurance of each (or as even as possible) of each channel
4. Pseudorandom

Approach:
  Initialise an array of channels are available

  While there are spaces left in the FHSSsequence array
    randomise the array of channels by iterating through and swapping each
    entry with another random entry. Skip the first entry, leaving it as
    the sync channel index (0).
    copy the array the the FHSS sequence, advance FHSS sequence offset
*/
void FHSSrandomiseFHSSsequence(long seed)
{

#ifdef Regulatory_Domain_AU_915
    Serial.println("Setting 915MHz Mode");
#elif defined Regulatory_Domain_FCC_915
    Serial.println("Setting 915MHz Mode");
#elif defined Regulatory_Domain_EU_868
    Serial.println("Setting 868MHz Mode");
#elif defined Regulatory_Domain_IN_866
    Serial.println("Setting 866MHz Mode");
#elif defined Regulatory_Domain_AU_433
    Serial.println("Setting 433MHz EU Mode");
#elif defined Regulatory_Domain_EU_433
    Serial.println("Setting 433MHz EU Mode");
#elif defined Regulatory_Domain_ISM_2400
    Serial.println("Setting 2400MHz Mode");
#else
#error No regulatory domain defined, please define one in common.h
#endif

    Serial.print("Number of FHSS frequencies = ");
    Serial.println(NR_FHSS_ENTRIES);

    rngSeed(seed);

    unsigned int sequencePosition = 0;

    uint8_t randomisedEntries[NR_FHSS_ENTRIES];
    // initialize array
    for (unsigned int i = 0; i < NR_FHSS_ENTRIES; i++)
    {
        randomisedEntries[i] = i; // assign the value to the sequence array
    }

    while (sequencePosition < NR_SEQUENCE_ENTRIES) {

        // randomise the next batch of entries by swapping, but skip the first entry
        for (unsigned int i = 1; i < NR_FHSS_ENTRIES; i++)
        {
            int rand = rngN(NR_FHSS_ENTRIES-1)+1; // random number between 1 and NR_FHSS_ENTRIES
            uint8_t temp = randomisedEntries[i];
            randomisedEntries[i] = randomisedEntries[rand]; // assign the value to the sequence array
            randomisedEntries[rand] = temp;
        }

        // set FHSS Sequence by copying the randomised array entries over
        for (unsigned int i = 0; i < NR_FHSS_ENTRIES && sequencePosition+i < NR_SEQUENCE_ENTRIES; i++)
        {
            FHSSsequence[sequencePosition+i] = randomisedEntries[i]; // assign the value to the sequence array

            // output FHSS sequence
            Serial.print(FHSSsequence[sequencePosition+i]);
            if ((sequencePosition+i + 1) % 10 == 0)
            {
                Serial.println();
            }
            else
            {
                Serial.print(" ");
            }

        }
        sequencePosition += NR_FHSS_ENTRIES;
    }

    Serial.println();
}

