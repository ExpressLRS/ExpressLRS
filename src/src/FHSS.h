#pragma once

#include "platform.h"
#include <stdint.h>

extern int32_t volatile FreqCorrection;
#define FreqCorrectionMax  100000
#define FreqCorrectionMin  -100000
#define FreqCorrectionStep 61 //min freq step is ~ 61hz

// The number of FHSS frequencies in the table
extern const uint32_t NR_FHSS_ENTRIES;
//#define NR_FHSS_ENTRIES (sizeof(FHSSfreqs) / sizeof(uint32_t))
extern const uint32_t FHSSfreqs[];

void ICACHE_RAM_ATTR FHSSresetFreqCorrection();

void ICACHE_RAM_ATTR FHSSsetCurrIndex(uint8_t value);
uint8_t ICACHE_RAM_ATTR FHSSgetCurrIndex();
void ICACHE_RAM_ATTR FHSSincCurrIndex();
uint8_t ICACHE_RAM_ATTR FHSSgetCurrSequenceIndex();

uint32_t ICACHE_RAM_ATTR GetInitialFreq();
uint32_t ICACHE_RAM_ATTR FHSSgetCurrFreq();
uint32_t ICACHE_RAM_ATTR FHSSgetNextFreq();
void FHSSrandomiseFHSSsequence();
