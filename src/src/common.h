#pragma once

// define exacly one frequency band of operation here

// #define Regulatory_Domain_AU_915
// #define Regulatory_Domain_EU_868
// #define Regulatory_Domain_AU_433
// #define Regulatory_Domain_EU_433
// #define Regulatory_Domain_FCC_915

#include "FHSS.h"
#include "LoRaRadioLib.h"
#include <Arduino.h>

// Wifi starts if no connection is found between 10 and 11 seconds after boot
#define Auto_WiFi_On_Boot

#define One_Bit_Switches

extern uint8_t UID[6];
extern uint8_t CRCCaesarCipher;
extern uint8_t DeviceAddr;

typedef enum
{
    TLM_RATIO_NO_TLM = 0,
    TLM_RATIO_1_128 = 1,
    TLM_RATIO_1_64 = 2,
    TLM_RATIO_1_32 = 3,
    TLM_RATIO_1_16 = 4,
    TLM_RATIO_1_8 = 5,
    TLM_RATIO_1_4 = 6,
    TLM_RATIO_1_2 = 7

} expresslrs_tlm_ratio_e;

typedef enum
{
    connected = 2,
    tentative = 1,
    disconnected = 0
} connectionState_e;

typedef enum
{
    tim_disconnected = 0,
    tim_tentative = 1,
    tim_locked = 2
} RXtimerState_e;

extern connectionState_e connectionState;
extern connectionState_e connectionStatePrev;

typedef enum
{
    RF_DOWNLINK_INFO = 0,
    RF_UPLINK_INFO = 1,
    RF_AIRMODE_PARAMETERS = 2
} expresslrs_tlm_header_e;

typedef enum
{
    RATE_200HZ = 0,
    RATE_100HZ = 1,
    RATE_50HZ = 2,
    RATE_25HZ = 3,
    RATE_4HZ = 4
} expresslrs_RFrates_e; // Max value of 16 since only 4 bits have been assigned in the sync package.

#define MaxRFrate 2

typedef struct expresslrs_mod_settings_s
{
    SX127x_Bandwidth bw;
    SX127x_SpreadingFactor sf;
    SX127x_CodingRate cr;
    int32_t sensitivity;                //expected RF sensitivity based on
    uint32_t interval;                  //interval in us seconds that corresponds to that frequnecy
    uint8_t rate;                       // rate in hz
    expresslrs_tlm_ratio_e TLMinterval; // every X packets is a response TLM packet, should be a power of 2
    uint8_t FHSShopInterval;            // every X packets we hope to a new frequnecy. Max value of 16 since only 4 bits have been assigned in the sync package.
    uint8_t PreambleLen;
    expresslrs_RFrates_e enum_rate; // Max value of 16 since only 4 bits have been assigned in the sync package.
    uint16_t RFmodeCycleAddtionalTime;
    uint16_t RFmodeCycleInterval;
} expresslrs_mod_settings_t;

expresslrs_mod_settings_s *get_elrs_airRateConfig(expresslrs_RFrates_e rate);

//extern const expresslrs_mod_settings_s * ExpressLRS_nextAirRate;
extern expresslrs_mod_settings_s *ExpressLRS_currAirRate;
extern expresslrs_mod_settings_s *ExpressLRS_prevAirRate;

extern int8_t ExpressLRS_currPower;
extern int8_t ExpressLRS_prevPower;

int16_t MeasureNoiseFloor();        //--todo, move this to radio lib
int16_t MeasureRSSI(int FHSSindex); //--todo, move this to radio lib

uint8_t TLMratioEnumToValue(expresslrs_tlm_ratio_e enumval);
