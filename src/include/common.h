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
extern uint16_t CRCInitializer;

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
    RATE_LORA_150HZ,
    RATE_LORA_200HZ,
    RATE_LORA_250HZ,
    RATE_LORA_500HZ,
    RATE_FLRC_500HZ,
    RATE_FLRC_1000HZ,
} expresslrs_RFrates_e; // Max value of 16 since only 4 bits have been assigned in the sync package.

enum {
    RADIO_TYPE_SX127x_LORA,
    RADIO_TYPE_SX128x_LORA,
    RADIO_TYPE_SX128x_FLRC,
};

// Value used for expresslrs_rf_pref_params_s.DynpowerUpThresholdSnr if SNR should not be used
#define DYNPOWER_SNR_THRESH_NONE -127
#define DYNPOWER_MEAN_ACCUMULATOR_MUL 4

typedef struct expresslrs_rf_pref_params_s
{
    uint8_t index;
    uint8_t enum_rate;                    // Max value of 4 since only 2 bits have been assigned in the sync package.
    int8_t RXsensitivity;                 // expected min RF sensitivity
    uint16_t TOA;                         // time on air in microseconds
    uint16_t DisconnectTimeoutMs;         // Time without a packet before receiver goes to disconnected (ms)
    uint16_t RxLockTimeoutMs;             // Max time to go from tentative -> connected state on receiver (ms)
    uint16_t SyncPktIntervalDisconnected; // how often to send the SYNC_PACKET packet (ms) when there is no response from RX
    uint16_t SyncPktIntervalConnected;    // how often to send the SYNC_PACKET packet (ms) when there we have a connection
    int8_t DynpowerSnrThreshUp;           // Request a raise in power if the reported (average) SNR is at or below this
                                          // or DYNPOWER_UPTHRESH_SNR_NONE to use RSSI
    int8_t DynpowerSnrThreshDn;           // Like DynpowerSnrUpThreshold except to lower power
} expresslrs_rf_pref_params_s;

typedef struct expresslrs_mod_settings_s
{
    uint8_t index;
    uint8_t radio_type;
    uint8_t enum_rate;          // Max value of 4 since only 2 bits have been assigned in the sync package.
    uint8_t bw;
    uint8_t sf;
    uint8_t cr;
    uint16_t interval;          // interval in us seconds that corresponds to that frequency
    uint8_t TLMinterval;        // every X packets is a response TLM packet, should be a power of 2
    uint8_t FHSShopInterval;    // every X packets we hop to a new frequency. Max value of 16 since only 4 bits have been assigned in the sync package.
    uint8_t PreambleLen;
    uint8_t PayloadLength;      // Number of OTA bytes to be sent.
} expresslrs_mod_settings_t;

#ifndef UNIT_TEST
#if defined(RADIO_SX127X)
#define RATE_MAX 4
#define RATE_DEFAULT 0
#define RATE_BINDING 2 // 50Hz bind mode

extern SX127xDriver Radio;

#elif defined(RADIO_SX128X)
#define RATE_MAX 6      // 2xFLRC + 4xLoRa
#define RATE_DEFAULT 0  // Default to FLRC 1000Hz
#define RATE_BINDING 5  // 50Hz bind mode

extern SX1280Driver Radio;
#endif


#define SYNC_PACKET_SWITCH_OFFSET   0   // Switch encoding mode
#define SYNC_PACKET_TLM_OFFSET      2   // Telemetry ratio
#define SYNC_PACKET_RATE_OFFSET     5   // Rate index
#define SYNC_PACKET_SWITCH_MASK     0b11
#define SYNC_PACKET_TLM_MASK        0b111
#define SYNC_PACKET_RATE_MASK       0b111


expresslrs_mod_settings_s *get_elrs_airRateConfig(uint8_t index);
expresslrs_rf_pref_params_s *get_elrs_RFperfParams(uint8_t index);

uint8_t TLMratioEnumToValue(uint8_t enumval);
uint16_t RateEnumToHz(uint8_t eRate);

extern expresslrs_mod_settings_s *ExpressLRS_currAirRate_Modparams;
extern expresslrs_rf_pref_params_s *ExpressLRS_currAirRate_RFperfParams;

uint8_t enumRatetoIndex(uint8_t rate);

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

//ELRS SPECIFIC OTA CRC
//Koopman formatting https://users.ece.cmu.edu/~koopman/crc/
#define ELRS_CRC_POLY 0x07 // 0x83
#define ELRS_CRC14_POLY 0x2E57 // 0x372B
