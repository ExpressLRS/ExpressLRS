#ifndef __FHSS_H
#define __FHSS_H

#include "platform.h"
#include <stdint.h>

extern int_fast32_t volatile DRAM_ATTR FreqCorrection;
#define FreqCorrectionMax 100000
#define FreqCorrectionMin -100000
#define FreqCorrectionStep 61 //min freq step is ~ 61hz

void ICACHE_RAM_ATTR FHSSresetFreqCorrection();

void ICACHE_RAM_ATTR FHSSsetCurrIndex(uint32_t value);
uint32_t ICACHE_RAM_ATTR FHSSgetCurrIndex();
void ICACHE_RAM_ATTR FHSSincCurrIndex();
uint8_t ICACHE_RAM_ATTR FHSSgetCurrSequenceIndex();

uint32_t ICACHE_RAM_ATTR GetInitialFreq();
uint32_t ICACHE_RAM_ATTR FHSSgetCurrFreq();
uint32_t ICACHE_RAM_ATTR FHSSgetNextFreq();
void FHSSrandomiseFHSSsequence();

#endif // __FHSS_H
