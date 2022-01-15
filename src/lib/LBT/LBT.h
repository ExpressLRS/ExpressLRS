#pragma once

#include "POWERMGNT.h"
#include "LQCALC.h"
#include "SX1280Driver.h"

extern LQCALC<100> LBTSuccessCalc;
extern bool LBTEnabled;

int SpreadingFactorToRSSIvalidDelayUs(SX1280_RadioLoRaSpreadingFactors_t SF);
int8_t ICACHE_RAM_ATTR PowerEnumToLBTLimit(PowerLevels_e txPower);
void ICACHE_RAM_ATTR BeginClearChannelAssessment(void);
bool ICACHE_RAM_ATTR ChannelIsClear(void);
void ICACHE_RAM_ATTR PrepareTXafterClearChannelAssessment(void);
void ICACHE_RAM_ATTR PrepareRXafterClearChannelAssessment(void);
