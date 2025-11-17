#pragma once
#include "SX12xxDriverCommon.h"

#if defined(Regulatory_Domain_EU_CE_2400)
#include "LQCALC.h"

extern LQCALC<100> LBTSuccessCalc;
extern bool LbtIsEnabled;

void LbtEnableIfRequired();
void ICACHE_RAM_ATTR LbtCcaTimerStart();
SX12XX_Radio_Number_t ICACHE_RAM_ATTR LbtChannelIsClear(SX12XX_Radio_Number_t radioNumber);
#else
static constexpr bool LbtIsEnabled = false;
static inline void LbtEnableIfRequired() {}
static inline void LbtCcaTimerStart() {}
static inline SX12XX_Radio_Number_t LbtChannelIsClear(SX12XX_Radio_Number_t radioNumber) { return radioNumber; }
#endif
