#pragma once
#include "SX12xxDriverCommon.h"

// in every build that can transmit at 2.4 GHz; firmwareOptions.lbt turns it on
#if defined(RADIO_SX128X) || defined(RADIO_LR1121) || defined(RADIO_LR2021)
#define HAS_LBT 1
#include "LQCALC.h"

extern LQCALC<100> LBTSuccessCalc;
extern bool LbtIsEnabled;

void LbtEnableIfRequired();
void ICACHE_RAM_ATTR LbtCcaTimerStart();
SX12XX_Radio_Number_t ICACHE_RAM_ATTR LbtChannelIsClear(SX12XX_Radio_Number_t radioNumber);
#else
#define HAS_LBT 0
static constexpr bool LbtIsEnabled = false;
static inline void LbtEnableIfRequired() {}
static inline void LbtCcaTimerStart() {}
static inline SX12XX_Radio_Number_t LbtChannelIsClear(SX12XX_Radio_Number_t radioNumber) { return radioNumber; }
#endif
