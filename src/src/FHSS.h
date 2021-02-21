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



uint8_t ICACHE_RAM_ATTR FHSSgetCurrIndex();

#ifndef SX127x_FREQ_STEP
#define SX127x_FREQ_STEP 61.03515625
#endif

#ifndef SX1280_FREQ_STEP
#define SX1280_FREQ_STEP ((double)(52000000 / pow(2.0, 18.0)))
#endif

// Our table of FHSS frequencies. Define a regulatory domain to select the correct set for your location and radio
#ifdef Regulatory_Domain_AU_433
const uint32_t FHSSfreqs[] = {
    (uint32_t)((double)433420000/SX127x_FREQ_STEP),
    (uint32_t)((double)433920000/SX127x_FREQ_STEP),
    (uint32_t)((double)434420000/SX127x_FREQ_STEP),
#elif defined Regulatory_Domain_AU_915
const uint32_t FHSSfreqs[] = {
    (uint32_t)((double)915500000/SX127x_FREQ_STEP),
    (uint32_t)((double)916100000/SX127x_FREQ_STEP),
    (uint32_t)((double)916700000/SX127x_FREQ_STEP),
    (uint32_t)((double)917300000/SX127x_FREQ_STEP),

    (uint32_t)((double)917900000/SX127x_FREQ_STEP),
    (uint32_t)((double)918500000/SX127x_FREQ_STEP),
    (uint32_t)((double)919100000/SX127x_FREQ_STEP),
    (uint32_t)((double)919700000/SX127x_FREQ_STEP),

    (uint32_t)((double)920300000/SX127x_FREQ_STEP),
    (uint32_t)((double)920900000/SX127x_FREQ_STEP),
    (uint32_t)((double)921500000/SX127x_FREQ_STEP),
    (uint32_t)((double)922100000/SX127x_FREQ_STEP),

    (uint32_t)((double)922700000/SX127x_FREQ_STEP),
    (uint32_t)((double)923300000/SX127x_FREQ_STEP),
    (uint32_t)((double)923900000/SX127x_FREQ_STEP),
    (uint32_t)((double)924500000/SX127x_FREQ_STEP),

    (uint32_t)((double)925100000/SX127x_FREQ_STEP),
    (uint32_t)((double)925700000/SX127x_FREQ_STEP),
    (uint32_t)((double)926300000/SX127x_FREQ_STEP),
    (uint32_t)((double)926900000/SX127x_FREQ_STEP)};
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
    (uint32_t)((double)863275000/SX127x_FREQ_STEP), // band H1, 863 - 865MHz, 0.1% duty cycle or CSMA techniques, 25mW EIRP
    (uint32_t)((double)863800000/SX127x_FREQ_STEP),
    (uint32_t)((double)864325000/SX127x_FREQ_STEP),
    (uint32_t)((double)864850000/SX127x_FREQ_STEP),
    (uint32_t)((double)865375000/SX127x_FREQ_STEP), // Band H2, 865 - 868.6MHz, 1.0% dutycycle or CSMA, 25mW EIRP
    (uint32_t)((double)865900000/SX127x_FREQ_STEP),
    (uint32_t)((double)866425000/SX127x_FREQ_STEP),
    (uint32_t)((double)866950000/SX127x_FREQ_STEP),
    (uint32_t)((double)867475000/SX127x_FREQ_STEP),
    (uint32_t)((double)868000000/SX127x_FREQ_STEP),
    (uint32_t)((double)868525000/SX127x_FREQ_STEP), // Band H3, 868.7-869.2MHz, 0.1% dutycycle or CSMA, 25mW EIRP
    (uint32_t)((double)869050000/SX127x_FREQ_STEP),
    (uint32_t)((double)869575000/SX127x_FREQ_STEP),};
#elif defined Regulatory_Domain_EU_433
/* Frequency band G, taken from https://wetten.overheid.nl/BWBR0036378/2016-12-28#Bijlagen
 * Note: As is the case with the 868Mhz band, these frequencies only comply to the license free portion
 * of the spectrum, nothing else. As such, these are likely illegal to use. 
 */
const uint32_t FHSSfreqs[] = {
    (uint32_t)((double)433100000/SX127x_FREQ_STEP),
    (uint32_t)((double)433925000/SX127x_FREQ_STEP),
    (uint32_t)((double)434450000/SX127x_FREQ_STEP)};
#elif defined Regulatory_Domain_FCC_915
/* Very definitely not fully checked. An initial pass at increasing the hops
*/
const uint32_t FHSSfreqs[] = {
    (uint32_t)((double)903500000/SX127x_FREQ_STEP),
    (uint32_t)((double)904100000/SX127x_FREQ_STEP),
    (uint32_t)((double)904700000/SX127x_FREQ_STEP),
    (uint32_t)((double)905300000/SX127x_FREQ_STEP),

    (uint32_t)((double)905900000/SX127x_FREQ_STEP),
    (uint32_t)((double)906500000/SX127x_FREQ_STEP),
    (uint32_t)((double)907100000/SX127x_FREQ_STEP),
    (uint32_t)((double)907700000/SX127x_FREQ_STEP),

    (uint32_t)((double)908300000/SX127x_FREQ_STEP),
    (uint32_t)((double)908900000/SX127x_FREQ_STEP),
    (uint32_t)((double)909500000/SX127x_FREQ_STEP),
    (uint32_t)((double)910100000/SX127x_FREQ_STEP),

    (uint32_t)((double)910700000/SX127x_FREQ_STEP),
    (uint32_t)((double)911300000/SX127x_FREQ_STEP),
    (uint32_t)((double)911900000/SX127x_FREQ_STEP),
    (uint32_t)((double)912500000/SX127x_FREQ_STEP),

    (uint32_t)((double)913100000/SX127x_FREQ_STEP),
    (uint32_t)((double)913700000/SX127x_FREQ_STEP),
    (uint32_t)((double)914300000/SX127x_FREQ_STEP),
    (uint32_t)((double)914900000/SX127x_FREQ_STEP),

    (uint32_t)((double)915500000/SX127x_FREQ_STEP), // as per AU..
    (uint32_t)((double)916100000/SX127x_FREQ_STEP),
    (uint32_t)((double)916700000/SX127x_FREQ_STEP),
    (uint32_t)((double)917300000/SX127x_FREQ_STEP),

    (uint32_t)((double)917900000/SX127x_FREQ_STEP),
    (uint32_t)((double)918500000/SX127x_FREQ_STEP),
    (uint32_t)((double)919100000/SX127x_FREQ_STEP),
    (uint32_t)((double)919700000/SX127x_FREQ_STEP),

    (uint32_t)((double)920300000/SX127x_FREQ_STEP),
    (uint32_t)((double)920900000/SX127x_FREQ_STEP),
    (uint32_t)((double)921500000/SX127x_FREQ_STEP),
    (uint32_t)((double)922100000/SX127x_FREQ_STEP),

    (uint32_t)((double)922700000/SX127x_FREQ_STEP),
    (uint32_t)((double)923300000/SX127x_FREQ_STEP),
    (uint32_t)((double)923900000/SX127x_FREQ_STEP),
    (uint32_t)((double)924500000/SX127x_FREQ_STEP),

    (uint32_t)((double)925100000/SX127x_FREQ_STEP),
    (uint32_t)((double)925700000/SX127x_FREQ_STEP),
    (uint32_t)((double)926300000/SX127x_FREQ_STEP),
    (uint32_t)((double)926900000(uint32_t)SX127x_FREQ_STEP};
#elif Regulatory_Domain_ISM_2400
const uint32_t FHSSfreqs[] = {
    (uint32_t)((double)2400400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2401400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2402400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2403400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2404400000/SX1280_FREQ_STEP),

    (uint32_t)((double)2405400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2406400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2407400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2408400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2409400000/SX1280_FREQ_STEP),

    (uint32_t)((double)2410400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2411400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2412400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2413400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2414400000/SX1280_FREQ_STEP),

    (uint32_t)((double)2415400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2416400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2417400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2418400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2419400000/SX1280_FREQ_STEP),

    (uint32_t)((double)2420400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2421400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2422400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2423400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2424400000/SX1280_FREQ_STEP),

    (uint32_t)((double)2425400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2426400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2427400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2428400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2429400000/SX1280_FREQ_STEP),

    (uint32_t)((double)2430400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2431400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2432400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2433400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2434400000/SX1280_FREQ_STEP),

    (uint32_t)((double)2435400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2436400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2437400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2438400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2439400000/SX1280_FREQ_STEP),

    (uint32_t)((double)2440400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2441400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2442400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2443400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2444400000/SX1280_FREQ_STEP),

    (uint32_t)((double)2445400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2446400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2447400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2448400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2449400000/SX1280_FREQ_STEP),

    (uint32_t)((double)2450400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2451400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2452400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2453400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2454400000/SX1280_FREQ_STEP),

    (uint32_t)((double)2455400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2456400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2457400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2458400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2459400000/SX1280_FREQ_STEP),

    (uint32_t)((double)2460400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2461400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2462400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2463400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2464400000/SX1280_FREQ_STEP),

    (uint32_t)((double)2465400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2466400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2467400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2468400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2469400000/SX1280_FREQ_STEP),

    (uint32_t)((double)2470400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2471400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2472400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2473400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2474400000/SX1280_FREQ_STEP),
    
    (uint32_t)((double)2475400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2476400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2477400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2478400000/SX1280_FREQ_STEP),
    (uint32_t)((double)2479400000/SX1280_FREQ_STEP)};
#else
#error No regulatory domain defined, please define one in user_defines.txt
#endif

// The number of FHSS frequencies in the table
#define NR_FHSS_ENTRIES (sizeof(FHSSfreqs) / sizeof(uint32_t))

#define NR_SEQUENCE_ENTRIES 256
extern uint8_t FHSSsequence[NR_SEQUENCE_ENTRIES];
void FHSSrandomiseFHSSsequence();

inline void FHSSsetCurrIndex(uint8_t value)
{ // set the current index of the FHSS pointer
    FHSSptr = value;
}

inline uint8_t FHSSgetCurrIndex()
{ // get the current index of the FHSS pointer
    return FHSSptr;
}

inline uint32_t GetInitialFreq()
{
    return FHSSfreqs[0] - FreqCorrection;
}

inline uint32_t FHSSgetCurrFreq()
{
    return FHSSfreqs[FHSSsequence[FHSSptr]] - FreqCorrection;
}

inline uint32_t FHSSgetNextFreq()
{
    FHSSptr++;
    return FHSSgetCurrFreq();
}

