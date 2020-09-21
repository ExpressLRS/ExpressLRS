#pragma once

#include <Arduino.h>

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
#elif Regulatory_Domain_ISM_2400
#include "SX1280Driver.h"
#endif

#include "utils.h"
#include "common.h"

#ifdef Regulatory_Domain_AU_915
#define Regulatory_Domain_Index 1
#elif defined Regulatory_Domain_FCC_915
#define Regulatory_Domain_Index 2
#elif defined Regulatory_Domain_EU_868
#define Regulatory_Domain_Index 3
#elif defined Regulatory_Domain_AU_433
#define Regulatory_Domain_Index 4
#elif defined Regulatory_Domain_EU_433
#define Regulatory_Domain_Index 5
#elif defined Regulatory_Domain_ISM_2400
#define Regulatory_Domain_Index 6
#else
#define Regulatory_Domain_Index 7
#endif

extern volatile uint8_t FHSSptr;

extern uint8_t NumOfFHSSfrequencies;

extern int32_t FreqCorrection;
#define FreqCorrectionMax 200000
#define FreqCorrectionMin -200000

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
/* Frequency bands taken from https://wetten.overheid.nl/BWBR0036378/2016-12-28#Bijlagen
 * Note: these frequencies fall in the license free H-band, but in combination with 500kHz 
 * LoRa modem bandwidth used by ExpressLRS (EU allows up to 125kHz modulation BW only) they
 * will never pass RED certification and they are ILLEGAL to use. 
 * 
 * Therefore we simply maximize the usage of available spectrum so laboratory testing of the software won't disturb existing
 * 868MHz ISM band traffic too much.
 */
const uint32_t FHSSfreqs[] = {
    863275000, // band H1, 863 - 865MHz, 0.1% duty cycle or CSMA techniques, 25mW EIRP
    863800000,
    864325000,
    864850000,
    865375000, // Band H2, 865 - 868.6MHz, 1.0% dutycycle or CSMA, 25mW EIRP
    865900000,
    866425000,
    866950000,
    867475000,
    868000000,
    868525000, // Band H3, 868.7-869.2MHz, 0.1% dutycycle or CSMA, 25mW EIRP
    869050000,
    869575000};
#elif defined Regulatory_Domain_EU_433
/* Frequency band G, taken from https://wetten.overheid.nl/BWBR0036378/2016-12-28#Bijlagen
 * Note: As is the case with the 868Mhz band, these frequencies only comply to the license free portion
 * of the spectrum, nothing else. As such, these are likely illegal to use. 
 */
const uint32_t FHSSfreqs[] = {
    433100000,
    433925000,
    434450000};
#elif defined Regulatory_Domain_FCC_915
/* Very definitely not fully checked. An initial pass at increasing the hops
*/
const uint32_t FHSSfreqs[] = {
    903500000,
    904100000,
    904700000,
    905300000,

    905900000,
    906500000,
    907100000,
    907700000,

    908300000,
    908900000,
    909500000,
    910100000,

    910700000,
    911300000,
    911900000,
    912500000,

    913100000,
    913700000,
    914300000,
    914900000,

    915500000, // as per AU..
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
#elif Regulatory_Domain_ISM_2400
const uint32_t FHSSfreqs[] = {
    2400400000,
    2401400000,
    2402400000,
    2403400000,
    2404400000,

    2405400000,
    2406400000,
    2407400000,
    2408400000,
    2409400000,

    2410400000,
    2411400000,
    2412400000,
    2413400000,
    2414400000,

    2415400000,
    2416400000,
    2417400000,
    2418400000,
    2419400000,

    2420400000,
    2421400000,
    2422400000,
    2423400000,
    2424400000,

    2425400000,
    2426400000,
    2427400000,
    2428400000,
    2429400000,

    2430400000,
    2431400000,
    2432400000,
    2433400000,
    2434400000,

    2435400000,
    2436400000,
    2437400000,
    2438400000,
    2439400000,

    2440400000,
    2441400000,
    2442400000,
    2443400000,
    2444400000,

    2445400000,
    2446400000,
    2447400000,
    2448400000,
    2449400000,

    2450400000,
    2451400000,
    2452400000,
    2453400000,
    2454400000,

    2455400000,
    2456400000,
    2457400000,
    2458400000,
    2459400000,

    2460400000,
    2461400000,
    2462400000,
    2463400000,
    2464400000,

    2465400000,
    2466400000,
    2467400000,
    2468400000,
    2469400000,

    2470400000,
    2471400000,
    2472400000,
    2473400000,
    2474400000,
    
    2475400000,
    2476400000,
    2477400000,
    2478400000,
    2479400000};
#else
#error No regulatory domain defined, please define one in user_defines.txt
#endif

// The number of FHSS frequencies in the table
#define NR_FHSS_ENTRIES (sizeof(FHSSfreqs) / sizeof(uint32_t))

#define NR_SEQUENCE_ENTRIES 256
extern uint8_t FHSSsequence[NR_SEQUENCE_ENTRIES];

uint32_t ICACHE_RAM_ATTR GetInitialFreq();
uint32_t ICACHE_RAM_ATTR FHSSgetCurrFreq();
uint32_t ICACHE_RAM_ATTR FHSSgetNextFreq();
void FHSSrandomiseFHSSsequence();