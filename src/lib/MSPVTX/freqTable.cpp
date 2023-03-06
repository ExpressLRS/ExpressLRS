#include "freqTable.h"

uint8_t getFreqTableChannels(void)
{
    return CHANNEL_COUNT;
}

uint8_t getFreqTableBands(void)
{
    return FREQ_TABLE_SIZE / getFreqTableChannels();
}

uint16_t getFreqByIdx(uint8_t idx)
{
    return channelFreqTable[idx];
}

uint8_t channelFreqLabelByIdx(uint8_t idx)
{
    return channelFreqLabel[idx];
}

uint8_t getBandLetterByIdx(uint8_t idx)
{
    return bandLetter[idx];
}
