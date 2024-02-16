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

// Used to XOR with OtaCrcInitializer and macSeed to reduce compatibility with previous versions.
// It should be incremented when the OTA packet structure is modified.
#define OTA_VERSION_ID      3
#define UID_LEN             6

typedef enum : uint8_t
{
    TLM_RATIO_STD = 0,   // Use suggested ratio from ModParams
    TLM_RATIO_NO_TLM,
    TLM_RATIO_1_128,
    TLM_RATIO_1_64,
    TLM_RATIO_1_32,
    TLM_RATIO_1_16,
    TLM_RATIO_1_8,
    TLM_RATIO_1_4,
    TLM_RATIO_1_2,
    TLM_RATIO_DISARMED, // TLM_RATIO_STD when disarmed, TLM_RATIO_NO_TLM when armed
} expresslrs_tlm_ratio_e;

typedef enum
{
    connected,
    tentative,        // RX only
    awaitingModelId,  // TX only
    disconnected,
    MODE_STATES,
    // States below here are special mode states
    noCrossfire,
    wifiUpdate,
    serialUpdate,
    bleJoystick,
    // Failure states go below here to display immediately
    FAILURE_STATES,
    radioFailed,
    hardwareUndefined
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

typedef enum
{
    RF_DOWNLINK_INFO = 0,
    RF_UPLINK_INFO = 1,
    RF_AIRMODE_PARAMETERS = 2
} expresslrs_tlm_header_e;

typedef enum : uint8_t
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
    RATE_DVDA_250HZ,
    RATE_DVDA_500HZ,
    RATE_FLRC_500HZ,
    RATE_FLRC_1000HZ,
    RATE_DVDA_50HZ,
} expresslrs_RFrates_e; // Max value of 16 since only 4 bits have been assigned in the sync package.

enum {
    RADIO_TYPE_SX127x_LORA,
    RADIO_TYPE_SX128x_LORA,
    RADIO_TYPE_SX128x_FLRC,
};

typedef enum : uint8_t
{
    TX_RADIO_MODE_GEMINI = 0,
    TX_RADIO_MODE_ANT_1 = 1,
    TX_RADIO_MODE_ANT_2 = 2,
    TX_RADIO_MODE_SWITCH = 3
} tx_radio_mode_e;

// Value used for expresslrs_rf_pref_params_s.DynpowerUpThresholdSnr if SNR should not be used
#define DYNPOWER_SNR_THRESH_NONE -127
#define SNR_SCALE(snr) ((int8_t)((float)snr * RADIO_SNR_SCALE))
#define SNR_DESCALE(snrScaled) (snrScaled / RADIO_SNR_SCALE)
// Bound is any of the last 4 bytes nonzero (unbound is all zeroes)
#define UID_IS_BOUND(uid) (uid[2] != 0 && uid[3] != 0 && uid[4] != 0 && uid[5] != 0)

typedef struct expresslrs_rf_pref_params_s
{
    uint8_t index;
    expresslrs_RFrates_e enum_rate;
    int16_t RXsensitivity;                // expected min RF sensitivity
    uint16_t TOA;                         // time on air in microseconds
    uint16_t DisconnectTimeoutMs;         // Time without a packet before receiver goes to disconnected (ms)
    uint16_t RxLockTimeoutMs;             // Max time to go from tentative -> connected state on receiver (ms)
    uint16_t SyncPktIntervalDisconnected; // how often to send the PACKET_TYPE_SYNC (ms) when there is no response from RX
    uint16_t SyncPktIntervalConnected;    // how often to send the PACKET_TYPE_SYNC (ms) when there we have a connection
    int8_t DynpowerSnrThreshUp;           // Request a raise in power if the reported (average) SNR is at or below this
                                          // or DYNPOWER_UPTHRESH_SNR_NONE to use RSSI
    int8_t DynpowerSnrThreshDn;           // Like DynpowerSnrUpThreshold except to lower power

} expresslrs_rf_pref_params_s;

typedef struct expresslrs_mod_settings_s
{
    uint8_t index;
    uint8_t radio_type;
    expresslrs_RFrates_e enum_rate;
    uint8_t bw;
    uint8_t sf;
    uint8_t cr;
    expresslrs_tlm_ratio_e TLMinterval;        // every X packets is a response TLM packet, should be a power of 2
    uint8_t FHSShopInterval;    // every X packets we hop to a new frequency. Max value of 16 since only 4 bits have been assigned in the sync package.
    int32_t interval;           // interval in us seconds that corresponds to that frequency
    uint8_t PreambleLen;
    uint8_t PayloadLength;      // Number of OTA bytes to be sent.
    uint8_t numOfSends;         // Number of packets to send.
} expresslrs_mod_settings_t;

// Limited to 16 possible ACTIONs by config storage currently
typedef enum : uint8_t {
    ACTION_NONE,
    ACTION_INCREASE_POWER,
    ACTION_GOTO_VTX_BAND,
    ACTION_GOTO_VTX_CHANNEL,
    ACTION_SEND_VTX,
    ACTION_START_WIFI,
    ACTION_BIND,
    ACTION_RESET_REBOOT,

    ACTION_LAST
} action_e;

enum eServoOutputMode : uint8_t
{
    som50Hz,    // Hz modes are "Servo PWM" where the signal is 988-2012us
    som60Hz,    // and the mode sets the refresh interval
    som100Hz,   // 50Hz must be mode=0 for default in config
    som160Hz,
    som333Hz,
    som400Hz,
    som10KHzDuty,
    somOnOff,   // Digital 0/1 mode
    somDShot,   // DShot300
    somSerial,  // Serial TX or RX depending on pin
    somSCL,     // I2C clock signal
    somSDA,     // I2C data line
    somPwm,     // True PWM mode (NOT SUPPORTED)
};

enum eServoOutputFailsafeMode : uint8_t
{
    PWMFAILSAFE_SET_POSITION,  // user customizable pulse value
    PWMFAILSAFE_NO_PULSES,     // stop pulsing
    PWMFAILSAFE_LAST_POSITION, // continue to pulse last used value
};

enum eSerialProtocol : uint8_t
{
    PROTOCOL_CRSF,
    PROTOCOL_INVERTED_CRSF,
    PROTOCOL_SBUS,
    PROTOCOL_INVERTED_SBUS,
	PROTOCOL_SUMD,
    PROTOCOL_DJI_RS_PRO,
    PROTOCOL_HOTT_TLM
};

enum eFailsafeMode : uint8_t
{
    FAILSAFE_NO_PULSES,
    FAILSAFE_LAST_POSITION,
    FAILSAFE_SET_POSITION
};

enum eAuxChannels : uint8_t
{
    AUX1 = 4,
    AUX2 = 5,
    AUX3 = 6,
    AUX4 = 7,
    AUX5 = 8,
    AUX6 = 9,
    AUX7 = 10,
    AUX8 = 11,
    AUX9 = 12,
    AUX10 = 13,
    AUX11 = 14,
    AUX12 = 15,
    CRSF_NUM_CHANNELS = 16
};

//ELRS SPECIFIC OTA CRC
//Koopman formatting https://users.ece.cmu.edu/~koopman/crc/
#define ELRS_CRC_POLY 0x07 // 0x83
#define ELRS_CRC14_POLY 0x2E57 // 0x372B

#ifndef UNIT_TEST
#if defined(RADIO_SX127X)
#define RATE_MAX 6
#define RATE_BINDING RATE_LORA_50HZ

extern SX127xDriver Radio;

#elif defined(RADIO_SX128X)
#define RATE_MAX 10     // 2xFLRC + 2xDVDA + 4xLoRa + 2xFullRes
#define RATE_BINDING RATE_LORA_50HZ

extern SX1280Driver Radio;
#endif
#endif // UNIT_TEST

expresslrs_mod_settings_s *get_elrs_airRateConfig(uint8_t index);
expresslrs_rf_pref_params_s *get_elrs_RFperfParams(uint8_t index);
uint8_t get_elrs_HandsetRate_max(uint8_t rateIndex, uint32_t minInterval);

uint8_t TLMratioEnumToValue(expresslrs_tlm_ratio_e const enumval);
uint8_t TLMBurstMaxForRateRatio(uint16_t const rateHz, uint8_t const ratioDiv);
uint8_t enumRatetoIndex(expresslrs_RFrates_e const eRate);

extern uint8_t UID[UID_LEN];
extern bool connectionHasModelMatch;
extern bool InBindingMode;
extern uint8_t ExpressLRS_currTlmDenom;
extern connectionState_e connectionState;
extern expresslrs_mod_settings_s *ExpressLRS_currAirRate_Modparams;
extern expresslrs_rf_pref_params_s *ExpressLRS_currAirRate_RFperfParams;
extern uint32_t ChannelData[CRSF_NUM_CHANNELS]; // Current state of channels, CRSF format

uint32_t uidMacSeedGet();
bool isDualRadio();
void EnterBindingModeSafely(); // defined in rx_main/tx_main