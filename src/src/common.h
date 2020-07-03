#pragma once

#include <Arduino.h>
#include "FHSS.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
#endif

#if defined(Regulatory_Domain_ISM_2400)
#include "SX1280Driver.h"
#endif

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
    bad_sync_retry = 4,
    bad_sync = 3,
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
    RATE_500HZ = 0,
    RATE_250HZ = 1,
    RATE_200HZ = 2,
    RATE_150HZ = 3,
    RATE_100HZ = 4,
    RATE_50HZ = 5,
    RATE_25HZ = 6,
    RATE_4HZ = 7
} expresslrs_RFrates_e; // Max value of 16 since only 4 bits have been assigned in the sync package.

typedef struct expresslrs_rf_pref_params_s
{
    int8_t index;
    expresslrs_RFrates_e enum_rate; // Max value of 16 since only 4 bits have been assigned in the sync package.
    int32_t RXsensitivity;          //expected RF sensitivity based on
    uint32_t TOA;                   //time on air in microseconds
    uint32_t RFmodeCycleInterval;
    uint32_t RFmodeCycleAddtionalTime;
    uint32_t SyncPktIntervalDisconnected;
    uint32_t SyncPktIntervalConnected;

} expresslrs_rf_pref_params_s;

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#define RATE_MAX 5
#define RATE_DEFAULT 0
typedef struct expresslrs_mod_settings_s
{
    int8_t index;
    expresslrs_RFrates_e enum_rate; // Max value of 16 since only 4 bits have been assigned in the sync package.
    SX127x_Bandwidth bw;
    SX127x_SpreadingFactor sf;
    SX127x_CodingRate cr;
    uint32_t interval;                  //interval in us seconds that corresponds to that frequnecy
    expresslrs_tlm_ratio_e TLMinterval; // every X packets is a response TLM packet, should be a power of 2
    uint8_t FHSShopInterval;            // every X packets we hope to a new frequnecy. Max value of 16 since only 4 bits have been assigned in the sync package.
    uint8_t PreambleLen;

} expresslrs_mod_settings_t;

#endif

#if defined(Regulatory_Domain_ISM_2400)
#define RATE_MAX 4
#define RATE_DEFAULT 1
typedef struct expresslrs_mod_settings_s
{
    uint8_t index;
    expresslrs_RFrates_e enum_rate; // Max value of 16 since only 4 bits have been assigned in the sync package.
    SX1280_RadioLoRaBandwidths_t bw;
    SX1280_RadioLoRaSpreadingFactors_t sf;
    SX1280_RadioLoRaCodingRates_t cr;
    uint32_t interval;                  //interval in us seconds that corresponds to that frequnecy
    expresslrs_tlm_ratio_e TLMinterval; // every X packets is a response TLM packet, should be a power of 2
    uint8_t FHSShopInterval;            // every X packets we hope to a new frequnecy. Max value of 16 since only 4 bits have been assigned in the sync package.
    uint8_t PreambleLen;

} expresslrs_mod_settings_t;

#endif

expresslrs_mod_settings_s *get_elrs_airRateConfig(int8_t index);
expresslrs_rf_pref_params_s *get_elrs_RFperfParams(int8_t index);

uint8_t ICACHE_RAM_ATTR TLMratioEnumToValue(expresslrs_tlm_ratio_e enumval);

extern expresslrs_mod_settings_s *ExpressLRS_currAirRate_Modparams;
extern expresslrs_rf_pref_params_s *ExpressLRS_currAirRate_RFperfParams;
//extern expresslrs_mod_settings_s *ExpressLRS_nextAirRate;
//extern expresslrs_mod_settings_s *ExpressLRS_prevAirRate;

extern bool ExpressLRS_AirRateNeedsUpdate;
