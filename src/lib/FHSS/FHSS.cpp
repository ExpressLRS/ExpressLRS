#include "FHSS.h"
#include <string.h>

uint8_t volatile FHSSptr;
uint8_t FHSSsequence[NR_SEQUENCE_ENTRIES];
int32_t FreqCorrection;
uint_fast8_t sync_channel;


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


// The number of FHSS frequencies in the table
constexpr uint32_t NR_FHSS_ENTRIES = (sizeof(FHSSfreqs) / sizeof(uint32_t));
constexpr uint32_t SYNC_INTERVAL = NR_FHSS_ENTRIES;


// Set all of the flags in the array to true, except for the first one
// which corresponds to the sync channel and is never available for normal
// allocation.
void resetIsAvailable(uint8_t * const array, const uint8_t size)
{
    // Mark all other entires to free (1)
    memset(array, 1, size);
    // the sync channel is never considered available
    array[sync_channel] = 0;
}

/**
Requirements:
1. 0 every n hops
2. No two repeated channels
3. Equal occurance of each (or as even as possible) of each channel
4. Pesudorandom

Approach:
  Initialise an array of flags indicating which channels have not yet been assigned and a counter of how many channels are available
  Iterate over the FHSSsequence array using index
    if index is a multiple of SYNC_INTERVAL assign the sync channel index (0)
    otherwise, generate a random number between 0 and the number of channels left to be assigned
    find the index of the nth remaining channel
    if the index is a repeat, generate a new random number
    if the index is not a repeat, assing it to the FHSSsequence array, clear the availability flag and decrement the available count
    if there are no available channels left, reset the flags array and the count
*/
void FHSSrandomiseFHSSsequence(const uint32_t seed)
{
#ifdef Regulatory_Domain_AU_915
    Serial.println("Setting 915MHz Mode");
#elif defined Regulatory_Domain_FCC_915
    Serial.println("Setting 915MHz Mode");
#elif defined Regulatory_Domain_EU_868
    Serial.println("Setting 868MHz Mode");
#elif defined Regulatory_Domain_IN_866
    Serial.println("Setting 866MHz Mode");
#elif defined Regulatory_Domain_AU_433
    Serial.println("Setting 433MHz EU Mode");
#elif defined Regulatory_Domain_EU_433
    Serial.println("Setting 433MHz EU Mode");
#elif defined Regulatory_Domain_ISM_2400
    Serial.println("Setting 2400MHz Mode");
#else
#error No regulatory domain defined, please define one in common.h
#endif

    Serial.print("Number of FHSS frequencies = ");
    Serial.println(NR_FHSS_ENTRIES);

    sync_channel = NR_FHSS_ENTRIES / 2;
    Serial.print("Sync channel = ");
    Serial.println(sync_channel);

    rngSeed(seed);

    uint8_t isAvailable[NR_FHSS_ENTRIES];

    resetIsAvailable(isAvailable, NR_FHSS_ENTRIES);

    int32_t nLeft = NR_FHSS_ENTRIES - 1; // how many channels are left to be allocated. Does not include the sync channel
    uint32_t prev = sync_channel;        // needed to prevent repeats of the same index
    uint32_t index;
    int32_t c, found;

    // Fill the FHSSsequence with channel indices
    // The 0 index is special - the 'sync' channel. The sync channel appears every
    // syncInterval hops. The other channels are randomly distributed between the
    // sync channels
    for (uint32_t i = 0; i < NR_SEQUENCE_ENTRIES; i++)
    {
        if ((i % SYNC_INTERVAL) == 0)
        {
            // assign sync channel
            prev = sync_channel;
        }
        else
        {
            // pick one of the available channels. May need to loop to avoid repeats
            do
            {
                c = rngN(nLeft); // returnc 0<c<nLeft
                // find the c'th entry in the isAvailable array
                index = 0;
                found = 0;
                while (index < NR_FHSS_ENTRIES)
                {
                    if (isAvailable[index])
                    {
                        if (found == c)
                            break;
                        found++;
                    }
                    index++;
                }
                if (index == NR_FHSS_ENTRIES)
                {
                    // This should never happen
                    Serial.print("FAILED to find the available entry!\n");
                    // What to do? We don't want to hang as that will stop us getting to the wifi hotspot
                    // Use the sync channel
                    index = sync_channel;
                    break;
                }
            } while (index == prev); // can't use index if it repeats the previous value

            isAvailable[index] = 0;  // clear the flag
            prev = index;            // remember for next iteration
            nLeft--;                 // reduce the count of available channels
            if (nLeft == 0)
            {
                // we've assigned all of the channels, so reset for next cycle
                resetIsAvailable(isAvailable, NR_FHSS_ENTRIES);
                nLeft = NR_FHSS_ENTRIES - 1;
            }
        }

        FHSSsequence[i] = prev; // assign the value to the sequence array

        Serial.print(prev);
        if ((i + 1) % 10 == 0)
        {
            Serial.println();
        }
        else
        {
            Serial.print(" ");
        }
    } // for each element in FHSSsequence

    Serial.println();
}

uint32_t FHSSNumEntriess(void)
{
    return NR_FHSS_ENTRIES;
}
