volatile uint8_t linkQuality = 0;
volatile uint8_t linkQualityArray[100] = {0};
volatile uint8_t linkQualityArrayIndex = 0;

void ICACHE_RAM_ATTR LQ_nextPacket()
{
    linkQualityArrayIndex = (linkQualityArrayIndex + 1) % sizeof(linkQualityArray);
    linkQualityArray[linkQualityArrayIndex] = 0;
}

void ICACHE_RAM_ATTR LQ_setPacketState(uint8_t state = 1)
{
    linkQualityArray[linkQualityArrayIndex] = state;
}

int ICACHE_RAM_ATTR LQ_getlinkQuality()
{
    int LQ = 0;

    for (int i = 0; i < 100; i++)
    {
        LQ += linkQualityArray[i];
    }

    return LQ;
}

int ICACHE_RAM_ATTR LQ_reset()
{
    for (int i = 0; i < 100; i++)
    {
        linkQualityArray[i] = 1; // set all good by default
    }
    return 0;
}
