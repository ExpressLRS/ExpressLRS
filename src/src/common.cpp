#include "common.h"
#include "platform.h"
#include "utils.h"
#include "FHSS.h"
#include "crc.h"
#include <Arduino.h>

volatile uint8_t current_rate_config = RATE_DEFAULT;

//
// https://semtech.my.salesforce.com/sfc/p/#E0000000JelG/a/2R000000HUhK/6T9Vdb3_ldnElA8drIbPYjs1wBbhlWUXej8ZMXtZXOM
//
const expresslrs_mod_settings_s DRAM_ATTR ExpressLRS_AirRateConfig[RATE_MAX] = {
    /* 200Hz */
    {BW_500_00_KHZ, SF_6, CR_4_5, -112, 5000, 200, TLM_RATIO_1_64, FHSS_1, 8, RATE_200HZ, 3872, 1000, 1500, 250000u}, // 8B
    //{BW_500_00_KHZ, SF_6, CR_4_7, -112, 5000, 200, TLM_RATIO_1_64, FHSS_1, 8, RATE_200HZ, 4384, 1000, 1500, 250000u},
    /* 100Hz */
    {BW_500_00_KHZ, SF_7, CR_4_7, -117, 10000, 100, TLM_RATIO_1_32, FHSS_1, 8, RATE_100HZ, 8768, 1000, 2000, 500000u}, // 9B
    /* 50Hz */
    {BW_500_00_KHZ, SF_8, CR_4_8, -120, 20000, 50, TLM_RATIO_1_16, FHSS_1, 8, RATE_50HZ, 18560, 1000, 2500, 750000u}, // 11B

#if RATE_MAX > RATE_50HZ
    {BW_500_00_KHZ, SF_9, CR_4_8, -123, 40000, 25, TLM_RATIO_1_8, FHSS_1, 10, RATE_25HZ, 30980, 6000, 2500, 0},
#elif RATE_MAX > (RATE_50HZ + 1)
    {BW_500_00_KHZ, SF_12, CR_4_8, -131, 250000, 4, TLM_RATIO_NO_TLM, FHSS_1, 10, RATE_4HZ, 247810, 6000, 2500, 0},
#endif
};

const expresslrs_mod_settings_s *get_elrs_airRateConfig(uint8_t rate)
{
    if (RATE_MAX <= rate)
        return NULL;
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
#if 1
    // TX: 0x1d
    // RX: 0x61 -> NOK
    // RX: 0x1E, 0x1F, 0x2d, 0x3d, 0x4d, 0x6d -> OK
    // RX: 0x20, 0x30, 0x40 -> NOK
    uint8_t syncw = CalcCRC(UID, sizeof(UID)); //UID[4];
    if (syncw == SX127X_SYNC_WORD_LORAWAN)
        syncw += 0x1;
    return syncw;
#else
    return SX127X_SYNC_WORD;
#endif
}
