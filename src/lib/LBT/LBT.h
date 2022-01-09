#pragma once

#include "POWERMGNT.h"
#include "LQCALC.h"

extern LQCALC<100> LBTSuccessCalc;

int8_t ICACHE_RAM_ATTR PowerEnumToLBTLimit(PowerLevels_e txPower);
void ICACHE_RAM_ATTR BeginClearChannelAssessment(void);
bool ICACHE_RAM_ATTR ChannelIsClear(void);
void ICACHE_RAM_ATTR PrepareTXafterClearChannelAssessment(void);
void ICACHE_RAM_ATTR PrepareRXafterClearChannelAssessment(void);