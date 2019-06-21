#include <Arduino.h>

#define FHSS_SEQ_LEN_433 16

uint8_t FHSSptr = 0;

void FHSSsetCurrIndex(uint8_t value)
{ // get the current index of the FHSS pointer
    FHSSptr = value;
}

uint8_t FHSSgetCurrIndex()
{ // get the current index of the FHSS pointer
    return FHSSptr;
}

float FHSSfreqs_433[5] = {
    433420000.00f,
    433670000.00f,
    433920000.00f,
    434170000.00f,
    434420000.00f};

uint8_t FHSSsequence_433[16] = {
    0, 1, 0, 4, 3, 2, 1, 0, 1, 3, 4, 0, 2, 0, 3, 4};

float FHSSgetNextFreq()
{
    FHSSptr++;

    if (FHSSptr == FHSS_SEQ_LEN_433)
    {
        FHSSptr = 0;
    }

    return FHSSfreqs_433[FHSSsequence_433[FHSSptr]];
}

float FHSSgetCurrFreq()
{
    return FHSSfreqs_433[FHSSsequence_433[FHSSptr]];
}
