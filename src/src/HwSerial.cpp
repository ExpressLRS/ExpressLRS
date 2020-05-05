#include "HwSerial.h"
#include "FIFO.h"

static uint8_t DRAM_ATTR OutData[128];

size_t HwSerial::write(FIFO &fifo)
{
    size_t ret = 0;
    uint8_t peekVal = fifo.peek(); // check if we have data in the output FIFO that needs to be written
    if (peekVal > 0)
    {
        if (fifo.size() >= peekVal)
        {
            uint8_t OutPktLen = fifo.pop();

            fifo.popBytes(OutData, OutPktLen);
            ret = HwSerial::write(OutData, OutPktLen);
        }
    }
    return ret;
}
