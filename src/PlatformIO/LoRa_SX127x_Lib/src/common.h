#include "LoRa_SX127x.h"

#define FREQ_915
// #define FREQ_433

typedef enum
{
    RF_DOWNLINK_INFO,
    RF_UPLINK_INFO
} expresslrs_tlm_header_e;

typedef enum
{
    RATE_200HZ = 0,
    RATE_100HZ = 1,
    RATE_50HZ = 2,
    RATE_25HZ = 3,
    RATE_4HZ = 4

} expresslrs_RFrates_e;

typedef struct expresslrs_mod_settings_s
{
    Bandwidth bw;
    SpreadingFactor sf;
    CodingRate cr;
    uint32_t interval;       //interval in us seconds that corresponds to that frequnecy
    uint8_t rate;            // rate in hz
    uint8_t TLMinterval;     // every X packets is a response TLM packet, should be a power of 2
    uint8_t FHSShopInterval; // every X packets we hope to a new frequnecy
    uint8_t PreambleLen;
    expresslrs_RFrates_e enum_rate;

} expresslrs_mod_settings_t;

// const expresslrs_mod_settings_s ExpressLRS_AirRateConfig[6] =
//     {
//         {BW_500_00_KHZ, SF_6, CR_4_5, 4000},    //250hz //have to drop the coding rate to make this work
//         {BW_500_00_KHZ, SF_6, CR_4_7, 5000},    //200hz //otherwise, coding rate of 4/7 gives some forward ECC
//         {BW_500_00_KHZ, SF_7, CR_4_7, 10000},   //100hz // medium range mode
//         {BW_250_00_KHZ, SF_7, CR_4_7, 20000},   //50hz
//         {BW_250_00_KHZ, SF_8, CR_4_7, 40000},   //25hz
//         {BW_250_00_KHZ, SF_11, CR_4_5, 250000}, //4hz
// };

expresslrs_mod_settings_s RF_RATE_200HZ = {BW_500_00_KHZ, SF_6, CR_4_5, 5000, 200, 64, 8, 8, RATE_200HZ};
expresslrs_mod_settings_s RF_RATE_100HZ = {BW_500_00_KHZ, SF_7, CR_4_5, 10000, 100, 32, 4, 8, RATE_100HZ};
expresslrs_mod_settings_s RF_RATE_50HZ = {BW_500_00_KHZ, SF_8, CR_4_7, 20000, 50, 0, 2, 8, RATE_50HZ};
expresslrs_mod_settings_s RF_RATE_25HZ = {BW_250_00_KHZ, SF_8, CR_4_7, 40000, 25, 0, 2, 8, RATE_25HZ};
expresslrs_mod_settings_s RF_RATE_4HZ = {BW_250_00_KHZ, SF_11, CR_4_5, 250000, 4, 0, 2, 8, RATE_4HZ};

const expresslrs_mod_settings_s ExpressLRS_AirRateConfig[5] = {RF_RATE_200HZ, RF_RATE_100HZ, RF_RATE_50HZ, RF_RATE_25HZ, RF_RATE_4HZ};

expresslrs_mod_settings_s ExpressLRS_currAirRate;
expresslrs_mod_settings_s ExpressLRS_prevAirRate;

#define MaxPower100mW_Module 20
#define MaxPower1000mW_Module 30
#define RF_Gain 10

int8_t ExpressLRS_currPower = 0;
int8_t ExpressLRS_prevPower = 0;
