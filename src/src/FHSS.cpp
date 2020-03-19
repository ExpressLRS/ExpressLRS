#include "FHSS.h"

uint8_t volatile FHSSptr = 0;
uint8_t FHSSsequence[256] = {0};

//uint8_t NumOfFHSSfrequencies = 20;
int32_t FreqCorrection = 0;

void ICACHE_RAM_ATTR FHSSsetCurrIndex(uint8_t value)
{ // set the current index of the FHSS pointer
    FHSSptr = value;
}

uint8_t ICACHE_RAM_ATTR FHSSgetCurrIndex()
{ // get the current index of the FHSS pointer
    return FHSSptr;
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
    FHSSptr++;
    return FHSSgetCurrFreq();
}

void ICACHE_RAM_ATTR FHSSrandomiseFHSSsequence()
{
    Serial.print("Number of FHSS frequencies =");
    Serial.print(NR_FHSS_ENTRIES);
    Serial.print("FHSSsequence[] = ");

    long macSeed = ((long)UID[2] << 24) + ((long)UID[3] << 16) + ((long)UID[4] << 8) + UID[5];
    rngSeed(macSeed);

    const int hopSeqLength = 256;
    const int numOfFreqs = NR_FHSS_ENTRIES-1;
    const int limit = floor(hopSeqLength / numOfFreqs);

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
            while (prev_val == rand || tracker[rand] >= limit || rand == 0 || rand > numOfFreqs)
            {
                rand = rng5Bit();
            }
        }

        FHSSsequence[i] = rand;
        tracker[rand]++;
        prev_val = rand;

        Serial.print(FHSSsequence[i]);
        Serial.print(", ");
    }

/* Note DaBit: is it really necessary that this is different logic? FHSSsequence[0] is never 0, and it is just a starting frequency anyway? */

/*    int prev_val = rng0to2(); // Randomised so that FHSSsequence[0] can also be 0.
    int rand = 0;

    for (int i = 0; i < 256; i++)
    {
        while (prev_val == rand)
        {
            rand = rng0to2();
        }

        prev_val = rand;
        FHSSsequence[i] = rand;

        Serial.print(FHSSsequence[i]);
        Serial.print(", ");
    }
*/

    Serial.println("");
}