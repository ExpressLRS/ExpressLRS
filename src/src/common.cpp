#include "common.h"
#include "platform.h"
#include "utils.h"
#include "FHSS.h"
#include <Arduino.h>

volatile uint8_t current_rate_config = RATE_DEFAULT;

//
// https://semtech.my.salesforce.com/sfc/p/#E0000000JelG/a/2R000000HUhK/6T9Vdb3_ldnElA8drIbPYjs1wBbhlWUXej8ZMXtZXOM
//
const expresslrs_mod_settings_s ExpressLRS_AirRateConfig[RATE_MAX] = {
    /* 200Hz */
    {BW_500_00_KHZ, SF_6, CR_4_7, -112, 5000, 200, TLM_RATIO_1_64, FHSS_4, 8, RATE_200HZ, 1000, 1500}, // airtime: 4.380ms/8B
    /* 100Hz */
    {BW_500_00_KHZ, SF_7, CR_4_8, -117, 10000, 100, TLM_RATIO_1_32, FHSS_4, 8, RATE_100HZ, 2000, 2000}, // airtime =  9.280ms/9B
    /* 50Hz */
    //{BW_500_00_KHZ, SF_8, CR_4_7, -120, 20000, 50, TLM_RATIO_1_16, FHSS_2, 10, RATE_50HZ, 6000, 2500}, // airtime = 18.560ms/11B - ORIG
    {BW_500_00_KHZ, SF_8, CR_4_8, -120, 20000, 50, TLM_RATIO_1_16, FHSS_2, 9, RATE_50HZ, 6000, 2500}, // airtime = 19.07ms/11B

#if RATE_MAX > RATE_50HZ
    {BW_250_00_KHZ, SF_8, CR_4_8, -123, 40000, 25, TLM_RATIO_1_8, FHSS_2, 10, RATE_25HZ, 6000, 2500}, // airtime = 39.17ms/11B
#elif RATE_MAX > (RATE_50HZ + 1)
    {BW_250_00_KHZ, SF_11, CR_4_5, -131, 250000, 4, TLM_RATIO_NO_TLM, FHSS_2, 8, RATE_4HZ, 6000, 2500},
#endif
};

const expresslrs_mod_settings_s *get_elrs_airRateConfig(uint8_t rate)
{
    if (RATE_MAX <= rate)
        rate = (RATE_MAX - 1);
    return &ExpressLRS_AirRateConfig[rate];
}

volatile const expresslrs_mod_settings_s *ExpressLRS_currAirRate = NULL;

#ifndef MY_UID
#error "UID is mandatory!"
#else
uint8_t UID[6] = {MY_UID};
#endif

uint8_t const CRCCaesarCipher = UID[4];
uint8_t const DeviceAddr = (UID[5] & 0b00111111) << 2; // temporarily based on mac until listen before assigning method merged

#if 0
static uint8_t my_sync_word = UID[4]; //0 , SX127X_SYNC_WORD;
uint8_t getSyncWord(void)
{
    if (my_sync_word)
        return my_sync_word;
    uint8_t i,
        u, syncw = 0;
    for (u = 0; u < 10 && (syncw < 0x10 || syncw == SX127X_SYNC_WORD_LORAWAN); u++)
    {
        syncw = SX127X_SYNC_WORD + u;
        for (i = 0; i < sizeof(UID); i++)
            syncw ^= UID[i];
    }
    my_sync_word = syncw;
    return syncw;
}
#else
uint8_t getSyncWord(void)
{
    uint8_t syncw = UID[4];
    if (syncw == SX127X_SYNC_WORD_LORAWAN)
        syncw++;
    return syncw;
}
#endif

#define RSSI_FLOOR_NUM_READS 5 // number of times to sweep the noise foor to get avg. RSSI reading

int16_t MeasureNoiseFloor()
{
    int NUM_READS = RSSI_FLOOR_NUM_READS * NR_FHSS_ENTRIES;
    int32_t returnval = 0;

    for (uint32_t freq = 0; freq < NR_FHSS_ENTRIES; freq++)
    {
        FHSSsetCurrIndex(freq);
        Radio.SetMode(SX127X_CAD);

        for (int i = 0; i < RSSI_FLOOR_NUM_READS; i++)
        {
            returnval += Radio.GetCurrRSSI();
            delay(5);
        }
    }
    returnval /= NUM_READS;
    return (returnval);
}

uint16_t TLMratioEnumToValue(uint8_t enumval)
{
#if 0
    switch (enumval)
    {
    case TLM_RATIO_1_2:
        return 2;
        break;
    case TLM_RATIO_1_4:
        return 4;
        break;
    case TLM_RATIO_1_8:
        return 8;
        break;
    case TLM_RATIO_1_16:
        return 16;
        break;
    case TLM_RATIO_1_32:
        return 32;
        break;
    case TLM_RATIO_1_64:
        return 64;
        break;
    case TLM_RATIO_1_128:
        return 128;
        break;
    case TLM_RATIO_NO_TLM:
    default:
        return 0xFF;
    }
#else
    return (256u >> (enumval));
#endif
}

static bool ledState = false;
void led_set_state(bool state)
{
    ledState = state;
    platform_set_led(state);
}

void led_toggle(void)
{
    led_set_state(!ledState);
}
