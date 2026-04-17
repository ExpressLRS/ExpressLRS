#pragma once

/*
How to make ELRS V4 backwards compatible with V3

The strategy is to:
 * keep a copy of the old OTA function, I've called it "legacy"
 * there's a new seed generation function, the old one is only slightly different
 * but since the seed is different, the hop table is different, and the home channel is different
 * if we are successful in receiving V4, then we stick with V4, no problems
 * if we parse a packet and it is not V4, we check if it is V3, and success happens by chance (we still know the bind phrase matches)
 * if we get enough V3 packets, then we can use the old seed to regenerate the hop table, use the old home channel, and switch to legacy mode

The packet structures are so similar that the differences are essentially... ignored...
except the sync packet, which is decoded as v3, then repackaged as v4, and then processed as v4
also, channel 4 is the arming switch, if you are not using a flight controller, this is inconsequential
and Gemini will not work, and any newer additions to OTA functionality will not work

*/

#include <cstddef>

#include "OTA.h"
#include "crc.h"
#include "crsf_protocol.h"
#include "FIFO.h"
#include "SX12xxDriverCommon.h"

// the defines below are pulled from the old telemetry_protocol.h but they've changed in v4, so the v3 versions are redefined here
#define OTALEGACY_ELRS4_TELEMETRY_SHIFT 2
#define OTALEGACY_ELRS4_TELEMETRY_BYTES_PER_CALL 5
#define OTALEGACY_ELRS4_TELEMETRY_MAX_PACKAGES (255 >> OTALEGACY_ELRS4_TELEMETRY_SHIFT)
#define OTALEGACY_ELRS8_TELEMETRY_BYTES_PER_CALL 10 // some of this is consumed by LinkStats buuuut....
#define OTALEGACY_ELRS8_TELEMETRY_SHIFT 3
#define OTALEGACY_ELRS8_TELEMETRY_MAX_PACKAGES (255 >> OTALEGACY_ELRS8_TELEMETRY_SHIFT)
#define OTALEGACY_ELRS4_MSP_BYTES_PER_CALL 5
#define OTALEGACY_ELRS8_MSP_BYTES_PER_CALL 10
#define OTALEGACY_ELRS_MSP_BUFFER 65
#define OTALEGACY_ELRS_MSP_MAX_PACKAGES ((OTALEGACY_ELRS_MSP_BUFFER/OTALEGACY_ELRS4_MSP_BYTES_PER_CALL)+1)
#define OTALEGACY_AP_MAX_BUF_LEN  64

extern bool ota_isLegacy;
extern uint32_t ota_legacySyncHoldUntilMs;
extern bool ota_isLegacySyncHoldActive();
extern uint32_t OtaGetUidSeed_v3();
extern void OtaUpdateCrcInitFromUid_v3();
extern void ota_cntNewVersionPkts(); // call this when an non-legacy packet is validated
extern void ota_resetPktVersionCounters(); // call this when switching radio configs/rates
extern bool ICACHE_RAM_ATTR HandleSendTelemetryResponse_v3();
extern bool ICACHE_RAM_ATTR ProcessRFPacket_v3(SX12xxDriverCommon::rx_status const status);

extern uint16_t OtaCrcInitializer;
extern uint16_t OtaCrcInitializer_v3;

#define OTA4_CRC_CALC_LEN_v3    offsetof(OTA_Packet4_v3_s, crcLow)
#define OTA8_CRC_CALC_LEN_v3    offsetof(OTA_Packet8_v3_s, crc)

typedef struct {
    uint8_t fhssIndex;
    uint8_t nonce;
    uint8_t switchEncMode:1,
            newTlmRatio:3,
            rateIndex:4;
    uint8_t UID3;
    uint8_t UID4;
    uint8_t UID5;
} PACKED OTA_Sync_v3_s;

typedef struct {
    uint8_t uplink_RSSI_1:7,
            antenna:1;
    uint8_t uplink_RSSI_2:7,
            modelMatch:1;
    uint8_t lq:7,
            mspConfirm:1;
    int8_t SNR;
} PACKED OTA_LinkStats_v3_s;

typedef struct {
    uint8_t raw[5]; // 4x 10-bit channels, see PackUInt11ToChannels4x10 for encoding
} PACKED OTA_Channels_4x10_v3;

typedef struct {
    // The packet type must always be the low two bits of the first byte of the
    // packet to match the same placement in OTA_Packet8_s
    uint8_t type: 2,
            crcHigh: 6;
    union {
        /** PACKET_TYPE_RCDATA **/
        struct {
            OTA_Channels_4x10_v3 ch;
            uint8_t switches:7,
                    ch4:1;
        } rc;
        struct {
            uint32_t packetNum; // LittleEndian
            uint8_t free[2];
        } PACKED dbg_linkstats;
        /** PACKET_TYPE_MSP **/
        struct {
            uint8_t packageIndex:7,
                    tlmFlag:1;
            uint8_t payload[OTALEGACY_ELRS4_MSP_BYTES_PER_CALL];
        } msp_ul;
        /** PACKET_TYPE_SYNC **/
        OTA_Sync_v3_s sync;
        /** PACKET_TYPE_TLM **/
        struct {
            uint8_t type:OTALEGACY_ELRS4_TELEMETRY_SHIFT,
                    packageIndex:(8 - OTALEGACY_ELRS4_TELEMETRY_SHIFT);
            union {
                struct {
                    OTA_LinkStats_v3_s stats;
                    uint8_t free;
                } PACKED ul_link_stats;
                uint8_t payload[OTALEGACY_ELRS4_TELEMETRY_BYTES_PER_CALL];
            };
        } tlm_dl; // PACKET_TYPE_TLM
        /** PACKET_TYPE_AIRPORT **/
        struct {
            uint8_t type:OTALEGACY_ELRS4_TELEMETRY_SHIFT,
                    count:(8 - OTALEGACY_ELRS4_TELEMETRY_SHIFT);
            uint8_t payload[OTALEGACY_ELRS4_TELEMETRY_BYTES_PER_CALL];
        } PACKED airport;
    };
    uint8_t crcLow;
} PACKED OTA_Packet4_v3_s;

typedef struct {
    // Like OTA_Packet4_s **the type is always the low two bits of the first byte**,
    // but they are specified in each type in the union as the other bits there
    // are not universal
    union {
        /** PACKET_TYPE_RCDATA **/
        struct {
            uint8_t packetType: 2,
                    telemetryStatus: 1,
                    uplinkPower: 3, // CRSF_power_level - 1 (1-8 is 0-7 in the air)
                    isHighAux: 1, // true if chHigh are AUX6-9
                    ch4: 1;   // AUX1, included up here so ch0 starts on a byte boundary
            OTA_Channels_4x10_v3 chLow;  // CH0-CH3
            OTA_Channels_4x10_v3 chHigh; // AUX2-5 or AUX6-9
        } PACKED rc;
        struct {
            uint8_t packetType; // actually struct rc's first byte
            uint32_t packetNum; // LittleEndian
            uint8_t free[6];
        } PACKED dbg_linkstats;
        /** PACKET_TYPE_MSP **/
        struct {
            uint8_t packetType: 2,
                    packageIndex: 5,
                    tlmFlag: 1;
            uint8_t payload[OTALEGACY_ELRS8_MSP_BYTES_PER_CALL];
        } msp_ul;
        /** PACKET_TYPE_SYNC **/
        struct {
            uint8_t packetType; // only low 2 bits
            OTA_Sync_v3_s sync;
            uint8_t free[4];
        } PACKED sync;
        /** PACKET_TYPE_TLM **/
        struct {
            uint8_t packetType: 2,
                    containsLinkStats: 1,
                    packageIndex: 5;
            union {
                struct {
                    OTA_LinkStats_v3_s stats;
                    uint8_t payload[OTALEGACY_ELRS8_TELEMETRY_BYTES_PER_CALL - sizeof(OTA_LinkStats_v3_s)];
                } PACKED ul_link_stats; // containsLinkStats == true
                uint8_t payload[OTALEGACY_ELRS8_TELEMETRY_BYTES_PER_CALL]; // containsLinkStats == false
            };
        } PACKED tlm_dl;
        /** PACKET_TYPE_AIRPORT **/
        struct {
            uint8_t packetType: 2,
                    containsLinkStats: 1,
                    count: 5;
            uint8_t payload[OTALEGACY_ELRS8_TELEMETRY_BYTES_PER_CALL];
        } PACKED airport;
    };
    uint16_t crc;  // crc16 LittleEndian
} PACKED OTA_Packet8_v3_s;

typedef struct {
    union {
        OTA_Packet4_v3_s std;
        OTA_Packet8_v3_s full;
    };
} PACKED OTA_Packet_v3_s;

typedef enum : uint8_t
{
    RATE_v3_LORA_4HZ = 0,
    RATE_v3_LORA_25HZ,
    RATE_v3_LORA_50HZ,
    RATE_v3_LORA_100HZ,
    RATE_v3_LORA_100HZ_8CH,
    RATE_v3_LORA_150HZ,
    RATE_v3_LORA_200HZ,
    RATE_v3_LORA_250HZ,
    RATE_v3_LORA_333HZ_8CH,
    RATE_v3_LORA_500HZ,
    RATE_v3_DVDA_250HZ, // FLRC
    RATE_v3_DVDA_500HZ, // FLRC
    RATE_v3_FLRC_500HZ,
    RATE_v3_FLRC_1000HZ,
    RATE_v3_DVDA_50HZ,
    RATE_v3_LORA_200HZ_8CH,
    RATE_v3_FSK_2G4_DVDA_500HZ,
    RATE_v3_FSK_2G4_1000HZ,
    RATE_v3_FSK_900_1000HZ,
    RATE_v3_FSK_900_1000HZ_8CH,
} expresslrs_RFrates_v3_e;
