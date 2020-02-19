#pragma once

#include <Arduino.h>
#include "LoRaRadioLib.h"
#include "utils.h"
#include "common.h"

extern volatile uint8_t FHSSptr;

extern uint8_t NumOfFHSSfrequencies;

extern int32_t FreqCorrection;
#define FreqCorrectionMax 100000
#define FreqCorrectionMin -100000

void ICACHE_RAM_ATTR FHSSsetCurrIndex(uint8_t value);

uint8_t ICACHE_RAM_ATTR FHSSgetCurrIndex();

const uint32_t FHSSfreqs433[5] = {
    433420000,
    433920000,
    434420000};

const uint32_t FHSSfreqs915[20] = {
    915500000,
    916100000,
    916700000,
    917300000,

    917900000,
    918500000,
    919100000,
    919700000,

    920300000,
    920900000,
    921500000,
    922100000,

    922700000,
    923300000,
    923900000,
    924500000,

    925100000,
    925700000,
    926300000,
    926900000};

extern uint8_t FHSSsequence[256];

uint32_t ICACHE_RAM_ATTR GetInitialFreq();
uint32_t ICACHE_RAM_ATTR FHSSgetCurrFreq();
uint32_t ICACHE_RAM_ATTR FHSSgetNextFreq();
void ICACHE_RAM_ATTR FHSSrandomiseFHSSsequence();