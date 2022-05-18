#pragma once

#ifndef UNIT_TEST
#include "targets.h"

#if defined(RADIO_SX127X)
#include "SX127xDriver.h"
#elif defined(RADIO_SX128X)
#include "SX1280Driver.h"
#else
#error "Radio configuration is not valid!"
#endif

#endif // UNIT_TEST

extern uint8_t BindingUID[6];
extern uint8_t UID[6];
extern uint8_t MasterUID[6];

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
    connected,
    tentative,
    disconnected,
    disconnectPending, // used on modelmatch change to drop the connection
    MODE_STATES,
    // States below here are special mode states
    noCrossfire,
    wifiUpdate,
    bleJoystick,
    // Failure states go below here to display immediately
    FAILURE_STATES,
    radioFailed
} connectionState_e;

/**
 * On the TX, tracks what to do when the Tock timer fires
 **/
typedef enum
{
    ttrpTransmitting,     // Transmitting RC channels as normal
    ttrpPreReceiveGap,    // Has switched to Receive mode for telemetry, but in the gap between TX done and Tock
    ttrpExpectingTelem    // Still in Receive mode, Tock has fired, receiving telem as far as we know
} TxTlmRcvPhase_e;

typedef enum
{
    tim_disconnected = 0,
    tim_tentative = 1,
    tim_locked = 2
} RXtimerState_e;

extern connectionState_e connectionState;
extern bool connectionHasModelMatch;

typedef enum
{
    RF_DOWNLINK_INFO = 0,
    RF_UPLINK_INFO = 1,
    RF_AIRMODE_PARAMETERS = 2
} expresslrs_tlm_header_e;

typedef enum
{
    RATE_LORA_4HZ = 0,
    RATE_LORA_25HZ,
    RATE_LORA_50HZ,
    RATE_LORA_100HZ,
    RATE_LORA_100HZ_8CH,
    RATE_LORA_150HZ,
    RATE_LORA_200HZ,
    RATE_LORA_250HZ,
    RATE_LORA_333HZ_8CH,
    RATE_LORA_500HZ,
    RATE_FLRC_500HZ,
    RATE_FLRC_1000HZ,
} expresslrs_RFrates_e; // Max value of 16 since only 4 bits have been assigned in the sync package.

enum {
    RADIO_TYPE_SX127x_LORA,
    RADIO_TYPE_SX128x_LORA,
    RADIO_TYPE_SX128x_FLRC,
};

typedef struct expresslrs_rf_pref_params_s
{
    uint8_t index;
    expresslrs_RFrates_e enum_rate;
    int32_t RXsensitivity;                // expected RF sensitivity based on
    uint32_t TOA;                         // time on air in microseconds
    uint32_t DisconnectTimeoutMs;         // Time without a packet before receiver goes to disconnected (ms)
    uint32_t RxLockTimeoutMs;             // Max time to go from tentative -> connected state on receiver (ms)
    uint32_t SyncPktIntervalDisconnected; // how often to send the PACKET_TYPE_SYNC (ms) when there is no response from RX
    uint32_t SyncPktIntervalConnected;    // how often to send the PACKET_TYPE_SYNC (ms) when there we have a connection

} expresslrs_rf_pref_params_s;

typedef struct expresslrs_mod_settings_s
{
    uint8_t index;
    uint8_t radio_type;
    expresslrs_RFrates_e enum_rate;
    uint8_t bw;
    uint8_t sf;
    uint8_t cr;
    uint32_t interval;          // interval in us seconds that corresponds to that frequency
    uint8_t TLMinterval;        // every X packets is a response TLM packet, should be a power of 2
    uint8_t FHSShopInterval;    // every X packets we hop to a new frequency. Max value of 16 since only 4 bits have been assigned in the sync package.
    uint8_t PreambleLen;
    uint8_t PayloadLength;      // Number of OTA bytes to be sent.
} expresslrs_mod_settings_t;

#ifndef UNIT_TEST
#if defined(RADIO_SX127X)
#define RATE_MAX 5
#define RATE_DEFAULT 0
#define RATE_BINDING 3 // 50Hz bind mode

extern SX127xDriver Radio;

#elif defined(RADIO_SX128X)
#define RATE_MAX 8      // 2xFLRC + 4xLoRa
#define RATE_DEFAULT 2  // Default to LoRa 500Hz
#define RATE_BINDING 7  // 50Hz bind mode

extern SX1280Driver Radio;
#endif


expresslrs_mod_settings_s *get_elrs_airRateConfig(uint8_t index);
expresslrs_rf_pref_params_s *get_elrs_RFperfParams(uint8_t index);

uint8_t TLMratioEnumToValue(uint8_t const enumval);
uint8_t TLMBurstMaxForRateRatio(uint16_t const rateHz, uint8_t const ratioDiv);
uint16_t RateEnumToHz(expresslrs_RFrates_e const eRate);
uint8_t enumRatetoIndex(expresslrs_RFrates_e const eRate);

extern expresslrs_mod_settings_s *ExpressLRS_currAirRate_Modparams;
extern expresslrs_rf_pref_params_s *ExpressLRS_currAirRate_RFperfParams;

#endif // UNIT_TEST

uint32_t uidMacSeedGet(void);

#define AUX1 4
#define AUX2 5
#define AUX3 6
#define AUX4 7
#define AUX5 8
#define AUX6 9
#define AUX7 10
#define AUX8 11
#define AUX9 12
#define AUX10 13
#define AUX11 14
#define AUX12 15

// ELRS SPECIFIC OTA CRC
// Value is implicit leading 1, comment is Koopman formatting (implicit trailing 1) https://users.ece.cmu.edu/~koopman/crc/
#define ELRS_CRC_POLY 0x07 // 0x83
#define ELRS_CRC14_POLY 0x2E57 // 0x372b
#define ELRS_CRC16_POLY 0x3D65 // 0x9eb2