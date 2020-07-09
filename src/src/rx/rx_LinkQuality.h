#define LQ_BUFF_SIZE 100

//volatile uint8_t linkQuality = 0;
volatile uint8_t linkQualityArray[LQ_BUFF_SIZE] = {0};
volatile uint8_t linkQualityArrayIndex = 0;

void ICACHE_RAM_ATTR LQ_nextPacket()
{
    uint8_t index = linkQualityArrayIndex + 1;
    index %= LQ_BUFF_SIZE;
    linkQualityArray[index] = 0;
    linkQualityArrayIndex = index;

    //linkQualityArrayIndex = (++index) % LQ_BUFF_SIZE;
}

void ICACHE_RAM_ATTR LQ_packetAck(void)
{
    linkQualityArray[linkQualityArrayIndex] = 1;
}

void ICACHE_RAM_ATTR LQ_packetNack(void)
{
    linkQualityArray[linkQualityArrayIndex] = 0;
}

int ICACHE_RAM_ATTR LQ_getlinkQuality()
{
    int LQ = 0, size = LQ_BUFF_SIZE;
    while (0 <= (--size)) {
        LQ += linkQualityArray[size];
    }
    return LQ;
}

void ICACHE_RAM_ATTR LQ_reset()
{
    int size = LQ_BUFF_SIZE;
    //for (int i = 0; i < LQ_BUFF_SIZE; i++)
    while (0 <= (--size)) {
        linkQualityArray[size] = 1; // set all good by default
    };
}
