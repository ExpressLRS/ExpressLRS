#include "LoRa_SX127x.h"
void commonConfig();

typedef enum
{
    RF_DOWNLINK_INFO,
    RF_UPLINK_INFO
} expresslrs_tlm_header_e;

typedef struct expresslrs_mod_settings_s
{
    Bandwidth BW;
    SpreadingFactor SF;
    CodingRate CR;
    uint32_t INTERVAL; //interval in us seconds that corresponds to that frequnecy

} expresslrs_mod_settings_t;

const expresslrs_mod_settings_s ExpressLRS_AirRateConfig[6] =
    {
        {BW_500_00_KHZ, SF_6, CR_4_5, 4000},    //250hz //have to drop the coding rate to make this work
        {BW_500_00_KHZ, SF_6, CR_4_7, 5000},    //200hz //otherwise, coding rate of 4/7 gives some forward ECC
        {BW_500_00_KHZ, SF_7, CR_4_7, 10000},   //100hz
        {BW_250_00_KHZ, SF_7, CR_4_7, 20000},   //50hz
        {BW_250_00_KHZ, SF_8, CR_4_7, 40000},   //25hz
        {BW_250_00_KHZ, SF_11, CR_4_5, 250000}, //4hz
};

expresslrs_mod_settings_s ExpressLRS_AirRateConfigOneway = {BW_250_00_KHZ, SF_8, CR_4_7};

uint8_t ExpressLRS_currAirRate = 0;
uint8_t ExpressLRS_prevAirRate = 0;

#define MaxPower100mW_Module 20
#define MaxPower1000mW_Module 30

int8_t ExpressLRS_currPower = 0;
int8_t ExpressLRS_prevPower = 0;
