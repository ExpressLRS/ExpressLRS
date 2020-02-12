#include "common.h"

void commonConfig()
{ //settings common to both master and slave
}

extern SX127xDriver Radio;

// TODO: Validate values for RFmodeCycleAddtionalTime and RFmodeCycleInterval for rates lower than 50HZ
expresslrs_mod_settings_s RF_RATE_200HZ =  {BW_500_00_KHZ, SF_6,  CR_4_5, -112, 5000,   200, TLM_RATIO_1_64,   4, 8,  RATE_200HZ, 1000, 1500};
expresslrs_mod_settings_s RF_RATE_100HZ =  {BW_500_00_KHZ, SF_7,  CR_4_7, -117, 10000,  100, TLM_RATIO_1_32,   4, 8, RATE_100HZ, 2000, 2000};
expresslrs_mod_settings_s RF_RATE_50HZ =   {BW_500_00_KHZ, SF_8,  CR_4_7, -120, 20000,  50,  TLM_RATIO_1_16,   2, 10, RATE_50HZ, 6000, 2500};
expresslrs_mod_settings_s RF_RATE_25HZ =   {BW_250_00_KHZ, SF_8,  CR_4_7, -123, 40000,  25,  TLM_RATIO_NO_TLM, 2, 8,  RATE_25HZ, 6000, 2500};
expresslrs_mod_settings_s RF_RATE_4HZ =    {BW_250_00_KHZ, SF_11, CR_4_5, -131, 250000, 4,   TLM_RATIO_NO_TLM, 2, 8,  RATE_4HZ, 6000, 2500};

const expresslrs_mod_settings_s ExpressLRS_AirRateConfig[5] = {RF_RATE_200HZ, RF_RATE_100HZ, RF_RATE_50HZ, RF_RATE_25HZ, RF_RATE_4HZ};

expresslrs_mod_settings_s ExpressLRS_nextAirRate;
expresslrs_mod_settings_s ExpressLRS_currAirRate;
expresslrs_mod_settings_s ExpressLRS_prevAirRate;

int8_t ExpressLRS_currPower = 0;
int8_t ExpressLRS_prevPower = 0;

connectionState_e connectionState = disconnected;
connectionState_e connectionStatePrev = disconnected;

//uint8_t TxBaseMac[6] = {48, 174, 164, 200, 100, 50};
//uint8_t TxBaseMac[6] = {180, 230, 45, 152, 126, 65}; //sandro mac
uint8_t TxBaseMac[6] = {180, 230, 45, 152, 125, 173}; // Wez's MAC

uint8_t CRCCaesarCipher = TxBaseMac[4];

uint8_t DeviceAddr = TxBaseMac[5] & 0b111111; // temporarily based on mac until listen before assigning method merged

#define RSSI_FLOOR_NUM_READS 5 // number of times to sweep the noise foor to get avg. RSSI reading
#define MEDIAN_SIZE 20

int16_t MeasureNoiseFloor()
{
    int NUM_READS = RSSI_FLOOR_NUM_READS * NumOfFHSSfrequencies;
    float returnval = 0;

    for (int freq = 0; freq < NumOfFHSSfrequencies; freq++)
    {
        FHSSsetCurrIndex(freq);
        Radio.SetMode(SX127X_CAD);

        for (int i = 0; i < RSSI_FLOOR_NUM_READS; i++)
        {
            returnval = returnval + Radio.GetCurrRSSI();
            delay(5);
        }
    }
    returnval = returnval / NUM_READS;
    return (returnval);
}