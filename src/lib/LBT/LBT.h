#if defined(Regulatory_Domain_EU_CE_2400)
#pragma once

#include "POWERMGNT.h"
#include "LQCALC.h"
#include "SX12xxDriverCommon.h"

extern LQCALC<100> LBTSuccessCalc;

void EnableLBT();
bool getLBTEnabled();
void ICACHE_RAM_ATTR SetClearChannelAssessmentTime(void);
SX12XX_Radio_Number_t ICACHE_RAM_ATTR ChannelIsClear(SX12XX_Radio_Number_t radioNumber);
#else
inline void EnableLBT() {}
#endif
