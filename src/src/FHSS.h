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

// Our table of FHSS frequencies. Define a regulatory domain to select the correct set for your location and radio
#ifdef Regulatory_Domain_AU_433
const uint32_t FHSSfreqs[] = {
    433420000,
    433920000,
    434420000};
#elif defined Regulatory_Domain_AU_915
const uint32_t FHSSfreqs[] = {
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
#elif defined Regulatory_Domain_EU_868
const uint32_t FHSSfreqs[] = {
    863350000,                      // band H1, 863 - 865MHz, 0.1% duty cycle or CSMA techniques, 25mW EIRP
    863950000,                      
    864550000,
    865250000,                      // Band H2, 865 - 868.6MHz, 1.0% dutycycle or CSMA, 25mW EIRP
    865850000,
    866450000,
    867050000,
    867650000,
    868250000,
    868950000,                     // Band H3, 868.7-869.2MHz, 0.1% dutycycle or CSMA, 25mW EIRP
    869700000};
#else
    #error No regulatory domain defined, please define one in common.h
#endif

// The number of FHSS frequencies in the table
#define NR_FHSS_ENTRIES (sizeof(FHSSfreqs) / sizeof(uint32_t))

extern uint8_t FHSSsequence[256];

uint32_t ICACHE_RAM_ATTR GetInitialFreq();
uint32_t ICACHE_RAM_ATTR FHSSgetCurrFreq();
uint32_t ICACHE_RAM_ATTR FHSSgetNextFreq();
void ICACHE_RAM_ATTR FHSSrandomiseFHSSsequence();