volatile uint8_t linkQuality = 0;
volatile uint8_t linkQualityArray[100] = {0};
volatile uint32_t linkQualityArrayCounter = 0;
volatile uint8_t linkQualityArrayIndex = 0;

#include "common.h"

void ICACHE_RAM_ATTR incrementLQArray()
{
    linkQualityArrayCounter++;
    linkQualityArrayIndex = linkQualityArrayCounter % 100;
    linkQualityArray[linkQualityArrayIndex] = 0;
}

void ICACHE_RAM_ATTR addPacketToLQ()
{
    linkQualityArray[linkQualityArrayIndex] = 1;
}

int ICACHE_RAM_ATTR getRFlinkQuality()
{
    int LQ = 0;

    for (int i = 0; i < 100; i++)
    {
        LQ += linkQualityArray[i];
    }

    return LQ;
}

int ICACHE_RAM_ATTR LQreset()
{
    for (int i = 0; i < 100; i++)
    {
        linkQualityArray[i] = 0;
    }
    return 0;
}