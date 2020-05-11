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
    {BW_500_00_KHZ, SF_6, CR_4_7, -112, 5000, 200, TLM_RATIO_1_64, FHSS_1, 8, RATE_200HZ, SYNC_64, 1000, 1500}, // airtime: 4.380ms/8B
    /* 100Hz */
    {BW_500_00_KHZ, SF_7, CR_4_8, -117, 10000, 100, TLM_RATIO_1_32, FHSS_1, 8, RATE_100HZ, SYNC_32, 2000, 2000}, // airtime =  9.280ms/9B
    /* 50Hz */
    //{BW_500_00_KHZ, SF_8, CR_4_7, -120, 20000, 50, TLM_RATIO_1_16, FHSS_2, 10, RATE_50HZ, SYNC_16, 6000, 2500}, // airtime = 18.560ms/11B - ORIG
    {BW_500_00_KHZ, SF_8, CR_4_8, -120, 20000, 50, TLM_RATIO_1_16, FHSS_2, 9, RATE_50HZ, SYNC_16, 6000, 2500}, // airtime = 19.07ms/11B

#if RATE_MAX > RATE_50HZ
    {BW_250_00_KHZ, SF_8, CR_4_8, -123, 40000, 25, TLM_RATIO_1_8, FHSS_2, 10, RATE_25HZ, SYNC_8, 6000, 2500}, // airtime = 39.17ms/11B
#elif RATE_MAX > (RATE_50HZ + 1)
    {BW_250_00_KHZ, SF_11, CR_4_5, -131, 250000, 4, TLM_RATIO_NO_TLM, FHSS_2, 8, RATE_4HZ, SYNC_8, 6000, 2500},
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
uint8_t const DRAM_ATTR UID[6] = {MY_UID};
#endif

uint8_t const DRAM_ATTR CRCCaesarCipher = UID[4];
uint8_t const DRAM_ATTR DeviceAddr = (UID[5] & 0b00111111) << 2; // temporarily based on mac until listen before assigning method merged

uint8_t getSyncWord(void)
{
#if 0
    // TX: 0x1d
    // RX: 0x61 -> NOK
    // RX: 0x1E, 0x1F, 0x2d, 0x3d, 0x4d, 0x6d -> OK
    // RX: 0x20, 0x30, 0x40 -> NOK
    uint8_t syncw = UID[4];
    if (syncw == SX127X_SYNC_WORD_LORAWAN)
        syncw++;
    return syncw;
#else
    return SX127X_SYNC_WORD;
#endif
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
