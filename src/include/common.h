#pragma once

#ifndef UNIT_TEST
#include "targets.h"
#include <device.h>

#if defined(RADIO_SX127X)
#include "SX127xDriver.h"
#elif defined(RADIO_LR1121)
#include "LR1121Driver.h"
#elif defined(RADIO_SX128X)
#include "SX1280Driver.h"
#else
#error "Radio configuration is not valid!"
#endif
#else
#include <cstdint>
#endif // UNIT_TEST

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
    bleJoystick,
    NO_CONFIG_SAVE_STATES,
    wifiUpdate,
    serialUpdate,
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
    // RATE_MODULATION_BAND_RATE_MODE

    RATE_LORA_900_25HZ = 0,
    RATE_LORA_900_50HZ,
    RATE_LORA_900_100HZ,
    RATE_LORA_900_100HZ_8CH,
    RATE_LORA_900_150HZ,
    RATE_LORA_900_200HZ,
    RATE_LORA_900_200HZ_8CH,
    RATE_LORA_900_250HZ,
    RATE_LORA_900_333HZ_8CH,
    RATE_LORA_900_500HZ,
    RATE_LORA_900_50HZ_DVDA,
    RATE_FSK_900_1000HZ_8CH,

    RATE_LORA_2G4_25HZ = 20,
    RATE_LORA_2G4_50HZ,
    RATE_LORA_2G4_100HZ,
    RATE_LORA_2G4_100HZ_8CH,
    RATE_LORA_2G4_150HZ,
    RATE_LORA_2G4_200HZ,
    RATE_LORA_2G4_200HZ_8CH,
    RATE_LORA_2G4_250HZ,
    RATE_LORA_2G4_333HZ_8CH,
    RATE_LORA_2G4_500HZ,
    RATE_FLRC_2G4_250HZ_DVDA,
    RATE_FLRC_2G4_500HZ_DVDA,
    RATE_FLRC_2G4_500HZ,
    RATE_FLRC_2G4_1000HZ,
    RATE_FSK_2G4_250HZ_DVDA,
    RATE_FSK_2G4_500HZ_DVDA,
    RATE_FSK_2G4_1000HZ,

    RATE_LORA_DUAL_100HZ_8CH = 100,
    RATE_LORA_DUAL_150HZ,
} expresslrs_RFrates_e;

enum {
    RADIO_TYPE_SX127x_LORA,
    RADIO_TYPE_LR1121_LORA_900,
    RADIO_TYPE_LR1121_LORA_2G4,
    RADIO_TYPE_LR1121_GFSK_900,
    RADIO_TYPE_LR1121_GFSK_2G4,
    RADIO_TYPE_LR1121_LORA_DUAL,
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

typedef enum : uint8_t
{
    TX_NORMAL_MODE      = 0,
    TX_MAVLINK_MODE     = 1,
} tx_transmission_mode_e;

// Value used for expresslrs_rf_pref_params_s.DynpowerUpThresholdSnr if SNR should not be used
#define DYNPOWER_SNR_THRESH_NONE -127
#define SNR_SCALE(snr) ((int8_t)((float)snr * RADIO_SNR_SCALE))
#define SNR_DESCALE(snrScaled) (snrScaled / RADIO_SNR_SCALE)
// Bound is any of the last 4 bytes nonzero (unbound is all zeroes)
#define UID_IS_BOUND(uid) (uid[2] != 0 || uid[3] != 0 || uid[4] != 0 || uid[5] != 0)

typedef struct expresslrs_rf_pref_params_s
{
    uint8_t index;
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
    uint8_t PreambleLen;
#if defined(RADIO_LR1121)
    uint8_t bw2;
    uint8_t sf2;
    uint8_t cr2;
    uint8_t PreambleLen2;
#endif
    expresslrs_tlm_ratio_e TLMinterval;        // every X packets is a response TLM packet, should be a power of 2
    uint8_t FHSShopInterval;    // every X packets we hop to a new frequency. Max value of 16 since only 4 bits have been assigned in the sync package.
    int32_t interval;           // interval in us seconds that corresponds to that frequency
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
    ACTION_BLE_JOYSTICK,
    ACTION_RESET_REBOOT,

    ACTION_LAST
} action_e;

enum eServoOutputMode : uint8_t
{
    som50Hz = 0,    // 0:  50 Hz  | modes are "Servo PWM" where the signal is 988-2012us
    som60Hz,        // 1:  60 Hz  | and the mode sets the refresh interval
    som100Hz,       // 2:  100 Hz | must be mode=0 for default in config
    som160Hz,       // 3:  160Hz
    som333Hz,       // 4:  333Hz
    som400Hz,       // 5:  400Hz
    som10KHzDuty,   // 6:  10kHz duty
    somOnOff,       // 7:  Digital 0/1 mode
    somDShot,       // 8:  DShot300
    somDShot3D,     // 9:  DShot300 3D
    somSerial,      // 10:  primary Serial
    somSCL,         // 11: I2C clock signal
    somSDA,         // 12: I2C data line
    somPwm,         // 13: true PWM mode (NOT SUPPORTED)
#if defined(PLATFORM_ESP32)
    somSerial1RX,   // 14: secondary Serial RX
    somSerial1TX,   // 15: secondary Serial TX
#endif
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
    PROTOCOL_HOTT_TLM,
    PROTOCOL_MAVLINK,
    PROTOCOL_MSP_DISPLAYPORT,
    PROTOCOL_GPS
};

#if defined(PLATFORM_ESP32)
enum eSerial1Protocol : uint8_t
{
    PROTOCOL_SERIAL1_OFF,
    PROTOCOL_SERIAL1_CRSF,
    PROTOCOL_SERIAL1_INVERTED_CRSF,
    PROTOCOL_SERIAL1_SBUS,
    PROTOCOL_SERIAL1_INVERTED_SBUS,
	PROTOCOL_SERIAL1_SUMD,
    PROTOCOL_SERIAL1_DJI_RS_PRO,
    PROTOCOL_SERIAL1_HOTT_TLM,
    PROTOCOL_SERIAL1_TRAMP,
    PROTOCOL_SERIAL1_SMARTAUDIO,
    PROTOCOL_SERIAL1_MSP_DISPLAYPORT,
    PROTOCOL_SERIAL1_GPS
};
#endif

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
#define RATE_BINDING RATE_LORA_900_50HZ

extern SX127xDriver Radio;

#elif defined(RADIO_LR1121)
#define RATE_MAX 20
#define RATE_BINDING RATE_LORA_900_50HZ
#define RATE_DUALBAND_BINDING RATE_LORA_2G4_50HZ

extern LR1121Driver Radio;

#elif defined(RADIO_SX128X)
#define RATE_MAX 10     // 2xFLRC + 2xDVDA + 4xLoRa + 2xFullRes
#define RATE_BINDING RATE_LORA_2G4_50HZ

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
extern bool teamraceHasModelMatch;
extern bool InBindingMode;
extern uint8_t ExpressLRS_currTlmDenom;
extern expresslrs_mod_settings_s *ExpressLRS_currAirRate_Modparams;
extern expresslrs_rf_pref_params_s *ExpressLRS_currAirRate_RFperfParams;
extern uint32_t ChannelData[CRSF_NUM_CHANNELS]; // Current state of channels, CRSF format

extern connectionState_e connectionState;
#if !defined(UNIT_TEST)
inline void setConnectionState(connectionState_e newState) {
    connectionState = newState;
    devicesTriggerEvent(EVENT_CONNECTION_CHANGED);
}
#endif

uint32_t uidMacSeedGet();
bool isDualRadio();
void EnterBindingModeSafely(); // defined in rx_main/tx_main

#if defined(RADIO_LR1121)
bool isSupportedRFRate(uint8_t index);
#else
inline bool isSupportedRFRate(uint8_t index) { return true; }
#endif
