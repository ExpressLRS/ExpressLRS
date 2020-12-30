#include "LQCALC.h"

void ICACHE_RAM_ATTR LQCALC::init()
{
    this->init(100);
}

void ICACHE_RAM_ATTR LQCALC::init(uint8_t depth)
{
    this->LQcalcDepth = depth;
}

void ICACHE_RAM_ATTR LQCALC::inc()
{
    this->LQArrayCounter++;
    this->LQArrayIndex = this->LQArrayCounter % this->LQcalcDepth;
    this->LQArray[this->LQArrayIndex] = 0;
}

void ICACHE_RAM_ATTR LQCALC::add()
{
    this->LQArray[LQArrayIndex] = 1;
}

uint8_t ICACHE_RAM_ATTR LQCALC::getLQ()
{
    this->LQ = 0;

    for (int i = 0; i < this->LQcalcDepth; i++)
    {
        this->LQ += this->LQArray[i];
    }

    return (this->LQ) * (100 / this->LQcalcDepth);
}

void ICACHE_RAM_ATTR LQCALC::reset()
{
    for (int i = 0; i < this->LQcalcDepth; i++)
    {
        this->LQArray[i] = 0;
    }
}

bool ICACHE_RAM_ATTR LQCALC::packetReceivedForPreviousFrame()
{
    uint32_t prevIndex = (LQArrayIndex == 0) ? (this->LQcalcDepth - 1) : (LQArrayIndex - 1);

    return (LQArray[prevIndex] != 0);
}
