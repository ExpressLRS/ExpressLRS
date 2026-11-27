#include "freqTable.h"

uint8_t getFreqTableChannels(void)
{
    return CHANNEL_COUNT;
}

uint8_t getFreqTableBands(void)
{
    return FREQ_TABLE_SIZE / getFreqTableChannels();
}

// The index comes from MSP and is not trusted, return 0 for an invalid index
uint16_t getFreqByIdx(uint8_t idx)
{
    return idx < FREQ_TABLE_SIZE ? channelFreqTable[idx] : 0;
}

uint8_t channelFreqLabelByIdx(uint8_t idx)
{
    return idx < FREQ_TABLE_SIZE ? channelFreqLabel[idx] : 0;
}

uint8_t getBandLetterByIdx(uint8_t idx)
{
    return idx < sizeof(bandLetter) ? bandLetter[idx] : 0;
}
