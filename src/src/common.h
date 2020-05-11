#pragma once

#include "LoRaRadioLib.h"
#include <stdint.h>

// Wifi starts if no connection is found between 10 and 11 seconds after boot
#define Auto_WiFi_On_Boot

#define One_Bit_Switches

#define BUTTON_RESET_INTERVAL_RX 4000u  // Hold button for 4 sec to reboot RX
#define WEB_UPDATE_PRESS_INTERVAL 2000u // hold button for 2 sec to enable webupdate mode

typedef enum
{
    STATE_search_iteration_done = -2, // e.g. RX could start SW upgrade
    STATE_fw_upgrade = -1,
    STATE_disconnected = 0,
    STATE_tentative = 1,
    STATE_connected = 2,
} connectionState_e;
extern volatile connectionState_e connectionState;

extern uint8_t const DRAM_ATTR UID[6];
extern uint8_t const DRAM_ATTR CRCCaesarCipher;
extern uint8_t const DRAM_ATTR DeviceAddr;

// These are used as message type header
#define DEIVCE_ADDR_GET(_B) ((_B)&0b11111100)
#define DEIVCE_ADDR_GENERATE(_B) (_B)
#define TYPE_GET(_B) ((_B)&0b00000011)

uint8_t getSyncWord(void);

enum
{
    TLM_RATIO_NO_TLM = 0,
    TLM_RATIO_1_128 = 1,
    TLM_RATIO_1_64 = 2,
    TLM_RATIO_1_32 = 3,
    TLM_RATIO_1_16 = 4,
    TLM_RATIO_1_8 = 5,
    TLM_RATIO_1_4 = 6,
    TLM_RATIO_1_2 = 7,
    TLM_RATIO_MAX,
    TLM_RATIO_DEFAULT = 0xff,
};

enum
{
    FHSS_1 = 1,
    FHSS_2 = 2,
    FHSS_4 = 4,
    FHSS_8 = 8,
};

enum
{
    // Note: modulo bitmask!
    SYNC_64 = 0x3f,
    SYNC_32 = 0x1f,
    SYNC_16 = 0xf,
    SYNC_8 = 0x7,
};

typedef enum
{
    RF_DOWNLINK_INFO = 0,
    RF_UPLINK_INFO = 1,
    RF_AIRMODE_PARAMETERS = 2
} expresslrs_tlm_header_e;

enum
{
    RATE_200HZ,
    RATE_100HZ,
    RATE_50HZ,
    //RATE_25HZ,
    //RATE_4HZ,
    RATE_MAX
};

#define RATE_GET_OSD_NUM(_x) ((RATE_MAX + 1) - (_x))
#define RATE_DEFAULT RATE_200HZ

typedef struct expresslrs_mod_settings_s
{
    Bandwidth bw;
    SpreadingFactor sf;
    CodingRate cr;
    int32_t sensitivity;     // expected RF sensitivity based on
    uint32_t interval;       // interval in us seconds that corresponds to that frequnecy
    uint8_t rate;            // rate in hz
    uint8_t TLMinterval;     // every X packets is a response TLM packet, should be a power of 2
    uint8_t FHSShopInterval; // every X packets we hope to a new frequnecy.
    uint8_t PreambleLen;
    uint8_t enum_rate;
    uint8_t sync_interval;
    uint16_t RFmodeCycleAddtionalTime;
    uint16_t RFmodeCycleInterval;
} expresslrs_mod_settings_t;

const expresslrs_mod_settings_s *get_elrs_airRateConfig(uint8_t rate);

extern volatile const expresslrs_mod_settings_s *ExpressLRS_currAirRate;
extern volatile uint8_t current_rate_config;

uint16_t TLMratioEnumToValue(uint8_t enumval);

void forced_start(void);
void forced_stop(void);
