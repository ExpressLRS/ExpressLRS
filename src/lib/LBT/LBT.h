#if defined(Regulatory_Domain_EU_CE_2400)
#pragma once

#include "POWERMGNT.h"
#include "LQCALC.h"
#include "SX1280Driver.h"

extern LQCALC<100> LBTSuccessCalc;
extern bool LBTEnabled;

void ICACHE_RAM_ATTR SetClearChannelAssessmentTime(void);
void ICACHE_RAM_ATTR BeginClearChannelAssessment(void);
bool ICACHE_RAM_ATTR ChannelIsClear(void);
#endif
