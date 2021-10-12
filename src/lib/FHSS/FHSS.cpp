#include "FHSS.h"
#include "logging.h"
#include <string.h>

// Our table of FHSS frequencies. Define a regulatory domain to select the correct set for your location and radio
#ifdef Regulatory_Domain_AU_433
const uint32_t FHSSfreqs[] = {
    FREQ_HZ_TO_REG_VAL(433420000),
    FREQ_HZ_TO_REG_VAL(433920000),
    FREQ_HZ_TO_REG_VAL(434420000)};
#elif defined Regulatory_Domain_AU_915
const uint32_t FHSSfreqs[] = {
    FREQ_HZ_TO_REG_VAL(915500000),
    FREQ_HZ_TO_REG_VAL(916100000),
    FREQ_HZ_TO_REG_VAL(916700000),
    FREQ_HZ_TO_REG_VAL(917300000),

    FREQ_HZ_TO_REG_VAL(917900000),
    FREQ_HZ_TO_REG_VAL(918500000),
    FREQ_HZ_TO_REG_VAL(919100000),
    FREQ_HZ_TO_REG_VAL(919700000),

    FREQ_HZ_TO_REG_VAL(920300000),
    FREQ_HZ_TO_REG_VAL(920900000),
    FREQ_HZ_TO_REG_VAL(921500000),
    FREQ_HZ_TO_REG_VAL(922100000),

    FREQ_HZ_TO_REG_VAL(922700000),
    FREQ_HZ_TO_REG_VAL(923300000),
    FREQ_HZ_TO_REG_VAL(923900000),
    FREQ_HZ_TO_REG_VAL(924500000),

    FREQ_HZ_TO_REG_VAL(925100000),
    FREQ_HZ_TO_REG_VAL(925700000),
    FREQ_HZ_TO_REG_VAL(926300000),
    FREQ_HZ_TO_REG_VAL(926900000)};
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
    FREQ_HZ_TO_REG_VAL(863275000), // band H1, 863 - 865MHz, 0.1% duty cycle or CSMA techniques, 25mW EIRP
    FREQ_HZ_TO_REG_VAL(863800000),
    FREQ_HZ_TO_REG_VAL(864325000),
    FREQ_HZ_TO_REG_VAL(864850000),
    FREQ_HZ_TO_REG_VAL(865375000), // Band H2, 865 - 868.6MHz, 1.0% dutycycle or CSMA, 25mW EIRP
    FREQ_HZ_TO_REG_VAL(865900000),
    FREQ_HZ_TO_REG_VAL(866425000),
    FREQ_HZ_TO_REG_VAL(866950000),
    FREQ_HZ_TO_REG_VAL(867475000),
    FREQ_HZ_TO_REG_VAL(868000000),
    FREQ_HZ_TO_REG_VAL(868525000), // Band H3, 868.7-869.2MHz, 0.1% dutycycle or CSMA, 25mW EIRP
    FREQ_HZ_TO_REG_VAL(869050000),
    FREQ_HZ_TO_REG_VAL(869575000)};
#elif defined Regulatory_Domain_IN_866
/**
 * India currently delicensed the 865-867 MHz band with a maximum of 1W Transmitter power,
 * 4Watts Effective Radiated Power and 200Khz carrier bandwidth as per
 * https://dot.gov.in/sites/default/files/Delicensing%20in%20865-867%20MHz%20band%20%5BGSR%20564%20%28E%29%5D_0.pdf .
 * There is currently no mention of Direct-sequence spread spectrum,
 * So these frequencies are a subset of Regulatory_Domain_EU_868 frequencies.
 */
const uint32_t FHSSfreqs[] = {
    FREQ_HZ_TO_REG_VAL(865375000),
    FREQ_HZ_TO_REG_VAL(865900000),
    FREQ_HZ_TO_REG_VAL(866425000),
    FREQ_HZ_TO_REG_VAL(866950000)};
#elif defined Regulatory_Domain_KR_917
/**
 * Frequency bands for LoRa (LR-WPANs) in Republic of Korea (South Korea) are taken from TTAK.KO-06.0246/R1
 * https://www.tta.or.kr/data/ttas_view.jsp?&pk_num=TTAK.KO-06.0246%2FR1
 * Allowed frequencies: 917-923.5 Mhz, Max. allowed power is 25 mW
 * 10 channels are allowed for 600 kHz BW, calculated by 917.5+(n-1)*0.6 MHz
 */
const uint32_t FHSSfreqs[] = {
    FREQ_HZ_TO_REG_VAL(917500000),
    FREQ_HZ_TO_REG_VAL(918100000),
    FREQ_HZ_TO_REG_VAL(918700000),
    FREQ_HZ_TO_REG_VAL(919300000),
    FREQ_HZ_TO_REG_VAL(919900000),
    FREQ_HZ_TO_REG_VAL(920500000),
    FREQ_HZ_TO_REG_VAL(921100000),
    FREQ_HZ_TO_REG_VAL(921700000),
    FREQ_HZ_TO_REG_VAL(922300000),
    FREQ_HZ_TO_REG_VAL(922900000)};
#elif defined Regulatory_Domain_EU_433
/* Frequency band G, taken from https://wetten.overheid.nl/BWBR0036378/2016-12-28#Bijlagen
 * Note: As is the case with the 868Mhz band, these frequencies only comply to the license free portion
 * of the spectrum, nothing else. As such, these are likely illegal to use.
 */
const uint32_t FHSSfreqs[] = {
    FREQ_HZ_TO_REG_VAL(433100000),
    FREQ_HZ_TO_REG_VAL(433925000),
    FREQ_HZ_TO_REG_VAL(434450000)};
#elif defined Regulatory_Domain_FCC_915
/* Very definitely not fully checked. An initial pass at increasing the hops
*/
const uint32_t FHSSfreqs[] = {
    FREQ_HZ_TO_REG_VAL(903500000),
    FREQ_HZ_TO_REG_VAL(904100000),
    FREQ_HZ_TO_REG_VAL(904700000),
    FREQ_HZ_TO_REG_VAL(905300000),

    FREQ_HZ_TO_REG_VAL(905900000),
    FREQ_HZ_TO_REG_VAL(906500000),
    FREQ_HZ_TO_REG_VAL(907100000),
    FREQ_HZ_TO_REG_VAL(907700000),

    FREQ_HZ_TO_REG_VAL(908300000),
    FREQ_HZ_TO_REG_VAL(908900000),
    FREQ_HZ_TO_REG_VAL(909500000),
    FREQ_HZ_TO_REG_VAL(910100000),

    FREQ_HZ_TO_REG_VAL(910700000),
    FREQ_HZ_TO_REG_VAL(911300000),
    FREQ_HZ_TO_REG_VAL(911900000),
    FREQ_HZ_TO_REG_VAL(912500000),

    FREQ_HZ_TO_REG_VAL(913100000),
    FREQ_HZ_TO_REG_VAL(913700000),
    FREQ_HZ_TO_REG_VAL(914300000),
    FREQ_HZ_TO_REG_VAL(914900000),

    FREQ_HZ_TO_REG_VAL(915500000), // as per AU..
    FREQ_HZ_TO_REG_VAL(916100000),
    FREQ_HZ_TO_REG_VAL(916700000),
    FREQ_HZ_TO_REG_VAL(917300000),

    FREQ_HZ_TO_REG_VAL(917900000),
    FREQ_HZ_TO_REG_VAL(918500000),
    FREQ_HZ_TO_REG_VAL(919100000),
    FREQ_HZ_TO_REG_VAL(919700000),

    FREQ_HZ_TO_REG_VAL(920300000),
    FREQ_HZ_TO_REG_VAL(920900000),
    FREQ_HZ_TO_REG_VAL(921500000),
    FREQ_HZ_TO_REG_VAL(922100000),

    FREQ_HZ_TO_REG_VAL(922700000),
    FREQ_HZ_TO_REG_VAL(923300000),
    FREQ_HZ_TO_REG_VAL(923900000),
    FREQ_HZ_TO_REG_VAL(924500000),

    FREQ_HZ_TO_REG_VAL(925100000),
    FREQ_HZ_TO_REG_VAL(925700000),
    FREQ_HZ_TO_REG_VAL(926300000),
    FREQ_HZ_TO_REG_VAL(926900000)};
#elif Regulatory_Domain_ISM_2400
const uint32_t FHSSfreqs[] = {
    FREQ_HZ_TO_REG_VAL(2400400000),
    FREQ_HZ_TO_REG_VAL(2401400000),
    FREQ_HZ_TO_REG_VAL(2402400000),
    FREQ_HZ_TO_REG_VAL(2403400000),
    FREQ_HZ_TO_REG_VAL(2404400000),

    FREQ_HZ_TO_REG_VAL(2405400000),
    FREQ_HZ_TO_REG_VAL(2406400000),
    FREQ_HZ_TO_REG_VAL(2407400000),
    FREQ_HZ_TO_REG_VAL(2408400000),
    FREQ_HZ_TO_REG_VAL(2409400000),

    FREQ_HZ_TO_REG_VAL(2410400000),
    FREQ_HZ_TO_REG_VAL(2411400000),
    FREQ_HZ_TO_REG_VAL(2412400000),
    FREQ_HZ_TO_REG_VAL(2413400000),
    FREQ_HZ_TO_REG_VAL(2414400000),

    FREQ_HZ_TO_REG_VAL(2415400000),
    FREQ_HZ_TO_REG_VAL(2416400000),
    FREQ_HZ_TO_REG_VAL(2417400000),
    FREQ_HZ_TO_REG_VAL(2418400000),
    FREQ_HZ_TO_REG_VAL(2419400000),

    FREQ_HZ_TO_REG_VAL(2420400000),
    FREQ_HZ_TO_REG_VAL(2421400000),
    FREQ_HZ_TO_REG_VAL(2422400000),
    FREQ_HZ_TO_REG_VAL(2423400000),
    FREQ_HZ_TO_REG_VAL(2424400000),

    FREQ_HZ_TO_REG_VAL(2425400000),
    FREQ_HZ_TO_REG_VAL(2426400000),
    FREQ_HZ_TO_REG_VAL(2427400000),
    FREQ_HZ_TO_REG_VAL(2428400000),
    FREQ_HZ_TO_REG_VAL(2429400000),

    FREQ_HZ_TO_REG_VAL(2430400000),
    FREQ_HZ_TO_REG_VAL(2431400000),
    FREQ_HZ_TO_REG_VAL(2432400000),
    FREQ_HZ_TO_REG_VAL(2433400000),
    FREQ_HZ_TO_REG_VAL(2434400000),

    FREQ_HZ_TO_REG_VAL(2435400000),
    FREQ_HZ_TO_REG_VAL(2436400000),
    FREQ_HZ_TO_REG_VAL(2437400000),
    FREQ_HZ_TO_REG_VAL(2438400000),
    FREQ_HZ_TO_REG_VAL(2439400000),

    FREQ_HZ_TO_REG_VAL(2440400000),
    FREQ_HZ_TO_REG_VAL(2441400000),
    FREQ_HZ_TO_REG_VAL(2442400000),
    FREQ_HZ_TO_REG_VAL(2443400000),
    FREQ_HZ_TO_REG_VAL(2444400000),

    FREQ_HZ_TO_REG_VAL(2445400000),
    FREQ_HZ_TO_REG_VAL(2446400000),
    FREQ_HZ_TO_REG_VAL(2447400000),
    FREQ_HZ_TO_REG_VAL(2448400000),
    FREQ_HZ_TO_REG_VAL(2449400000),

    FREQ_HZ_TO_REG_VAL(2450400000),
    FREQ_HZ_TO_REG_VAL(2451400000),
    FREQ_HZ_TO_REG_VAL(2452400000),
    FREQ_HZ_TO_REG_VAL(2453400000),
    FREQ_HZ_TO_REG_VAL(2454400000),

    FREQ_HZ_TO_REG_VAL(2455400000),
    FREQ_HZ_TO_REG_VAL(2456400000),
    FREQ_HZ_TO_REG_VAL(2457400000),
    FREQ_HZ_TO_REG_VAL(2458400000),
    FREQ_HZ_TO_REG_VAL(2459400000),

    FREQ_HZ_TO_REG_VAL(2460400000),
    FREQ_HZ_TO_REG_VAL(2461400000),
    FREQ_HZ_TO_REG_VAL(2462400000),
    FREQ_HZ_TO_REG_VAL(2463400000),
    FREQ_HZ_TO_REG_VAL(2464400000),

    FREQ_HZ_TO_REG_VAL(2465400000),
    FREQ_HZ_TO_REG_VAL(2466400000),
    FREQ_HZ_TO_REG_VAL(2467400000),
    FREQ_HZ_TO_REG_VAL(2468400000),
    FREQ_HZ_TO_REG_VAL(2469400000),

    FREQ_HZ_TO_REG_VAL(2470400000),
    FREQ_HZ_TO_REG_VAL(2471400000),
    FREQ_HZ_TO_REG_VAL(2472400000),
    FREQ_HZ_TO_REG_VAL(2473400000),
    FREQ_HZ_TO_REG_VAL(2474400000),

    FREQ_HZ_TO_REG_VAL(2475400000),
    FREQ_HZ_TO_REG_VAL(2476400000),
    FREQ_HZ_TO_REG_VAL(2477400000),
    FREQ_HZ_TO_REG_VAL(2478400000),
    FREQ_HZ_TO_REG_VAL(2479400000)};
#else
#error No regulatory domain defined, please define one in user_defines.txt
#endif

// Number of FHSS frequencies in the table
constexpr uint32_t FHSS_FREQ_CNT = (sizeof(FHSSfreqs) / sizeof(uint32_t));
// Number of hops in the FHSSsequence list before circling back around, even multiple of the number of frequencies
constexpr uint8_t  FHSS_SEQUENCE_CNT = (256 / FHSS_FREQ_CNT) * FHSS_FREQ_CNT;
// Actual sequence of hops as indexes into the frequency list
uint8_t FHSSsequence[FHSS_SEQUENCE_CNT];
// Which entry in the sequence we currently are on
uint8_t volatile FHSSptr;
// Channel for sync packets and initial connection establishment
uint_fast8_t sync_channel;
// Offset from the predefined frequency determined by AFC on Team900 (register units)
int32_t FreqCorrection;

/**
Requirements:
1. 0 every n hops
2. No two repeated channels
3. Equal occurance of each (or as even as possible) of each channel
4. Pseudorandom

Approach:
  Fill the sequence array with the sync channel every FHSS_FREQ_CNT
  Iterate through the array, and for each block, swap each entry in it with
  another random entry, excluding the sync channel.

*/
void FHSSrandomiseFHSSsequence(const uint32_t seed)
{
#ifdef Regulatory_Domain_AU_915
    INFOLN("Setting 915MHz AU Mode");
#elif defined Regulatory_Domain_FCC_915
    INFOLN("Setting 915MHz FCC Mode");
#elif defined Regulatory_Domain_EU_868
    INFOLN("Setting 868MHz EU Mode");
#elif defined Regulatory_Domain_IN_866
    INFOLN("Setting 866MHz IN Mode");
#elif defined Regulatory_Domain_AU_433
    INFOLN("Setting 433MHz AU Mode");
#elif defined Regulatory_Domain_EU_433
    INFOLN("Setting 433MHz EU Mode");
#elif defined Regulatory_Domain_KR_917
    INFOLN("Setting 917MHz KR Mode");
#elif defined Regulatory_Domain_ISM_2400
    INFOLN("Setting 2400MHz Mode");
#else
#error No regulatory domain defined, please define one in common.h
#endif

    DBGLN("Number of FHSS frequencies = %u", FHSS_FREQ_CNT);

    sync_channel = FHSS_FREQ_CNT / 2;
    DBGLN("Sync channel = %u", sync_channel);

    // reset the pointer (otherwise the tests fail)
    FHSSptr = 0;
    rngSeed(seed);

    // initialize the sequence array
    for (uint8_t i = 0; i < FHSS_SEQUENCE_CNT; i++)
    {
        if (i % FHSS_FREQ_CNT == 0) {
            FHSSsequence[i] = sync_channel;
        } else if (i % FHSS_FREQ_CNT == sync_channel) {
            FHSSsequence[i] = 0;
        } else {
            FHSSsequence[i] = i % FHSS_FREQ_CNT;
        }
    }

    for (uint8_t i=0; i < FHSS_SEQUENCE_CNT; i++)
    {
        // if it's not the sync channel
        if (i % FHSS_FREQ_CNT != 0)
        {
            uint8_t offset = (i / FHSS_FREQ_CNT) * FHSS_FREQ_CNT; // offset to start of current block
            uint8_t rand = rngN(FHSS_FREQ_CNT-1)+1; // random number between 1 and FHSS_FREQ_CNT

            // switch this entry and another random entry in the same block
            uint8_t temp = FHSSsequence[i];
            FHSSsequence[i] = FHSSsequence[offset+rand];
            FHSSsequence[offset+rand] = temp;
        }
    }

    // output FHSS sequence
    for (uint8_t i=0; i < FHSS_SEQUENCE_CNT; i++)
    {
        DBG("%u ",FHSSsequence[i]);
        if (i % 10 == 9)
            DBGCR;
    }
    DBGCR;
}

uint32_t FHSSgetChannelCount(void)
{
    return FHSS_FREQ_CNT;
}
