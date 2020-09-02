#pragma once

#include <Arduino.h>
#include <stdio.h>
#include "../../src/targets.h"

class LQCALC
{
public:
    static LQCALC *instance;
    void init(uint8_t depth);
    void init();
    void ICACHE_RAM_ATTR add();
    void ICACHE_RAM_ATTR inc();
    uint8_t ICACHE_RAM_ATTR getLQ();
    void ICACHE_RAM_ATTR reset();

private:
    uint8_t LQcalcDepth = 0;
    uint8_t LQ = 0;
    uint8_t LQArray[100] = {0};
    uint32_t LQArrayCounter = 0;
    uint8_t LQArrayIndex = 0;
};