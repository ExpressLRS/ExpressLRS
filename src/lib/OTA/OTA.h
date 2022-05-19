#ifndef H_OTA
#define H_OTA

#include <functional>
#include "common.h"
#include "crc.h"
#include "CRSF.h"

#define OTA4_PACKET_SIZE     8U
#define OTA4_CRC_CALC_LEN    offsetof(OTA_Packet4_s, crcLow)
#define OTA8_PACKET_SIZE     13U
#define OTA8_CRC_CALC_LEN    offsetof(OTA_Packet8_s, crc)

// Packet header types (ota.std.type)
#define PACKET_TYPE_RCDATA  0b00
#define PACKET_TYPE_MSPDATA 0b01
#define PACKET_TYPE_TLM     0b11
#define PACKET_TYPE_SYNC    0b10

// Mask used to XOR the ModelId into the SYNC packet for ModelMatch
#define MODELMATCH_MASK 0x3f

typedef struct {
    uint8_t fhssIndex;
    uint8_t nonce;
    uint8_t switchEncMode:1,
            newTlmRatio:3,
            rateIndex:4;
    uint8_t UID3;
    uint8_t UID4;
    uint8_t UID5;
} PACKED OTA_Sync_s;

typedef struct {
    uint8_t uplink_RSSI_1:7,
            antenna:1;
    uint8_t uplink_RSSI_2:7,
            modelMatch:1;
    uint8_t lq:7,
            mspConfirm:1;
    uint8_t SNR; // only needs about 5 of these bits
} PACKED OTA_LinkStats_s;

typedef struct {
    uint8_t raw[5]; // 4x 10-bit channels, see PackUInt11ToChannels4x10 for encoding
} PACKED OTA_Channels_4x10;

typedef struct {
    // The packet type must always be the low two bits of the first byte of the
    // packet to match the same placement in OTA_Packet8_s
    uint8_t type: 2,
            crcHigh: 6;
    union {
        /** PACKET_TYPE_RCDATA **/
        struct {
            OTA_Channels_4x10 ch;
            uint8_t switches:7,
                    ch4:1;
        } rc;
        struct {
            uint32_t packetNum; // LittleEndian
            uint8_t free[2];
        } PACKED dbg_linkstats;
        /** PACKET_TYPE_MSP **/
        struct {
            uint8_t packageIndex;
            uint8_t payload[ELRS_MSP_BYTES_PER_CALL];
        } msp_ul;
        /** PACKET_TYPE_SYNC **/
        OTA_Sync_s sync;
        /** PACKET_TYPE_TLM **/
        struct {
            uint8_t type:ELRS4_TELEMETRY_SHIFT,
                    packageIndex:(8 - ELRS4_TELEMETRY_SHIFT);
            union {
                struct {
                    OTA_LinkStats_s stats;
                    uint8_t free;
                } PACKED ul_link_stats;
                uint8_t payload[ELRS4_TELEMETRY_BYTES_PER_CALL];
            };
        } tlm_dl; // PACKET_TYPE_TLM
    };
    uint8_t crcLow;
} PACKED OTA_Packet4_s;

typedef struct {
    // Like OTA_Packet4_s **the type is always the low two bits of the first byte**,
    // but they are speficied in each type in the union as the other bits there
    // are not universal
    union {
        /** PACKET_TYPE_RCDATA **/
        struct {
            uint8_t packetType: 2,
                    telemetryStatus: 1,
                    uplinkPower: 3, // PowerLevels_e
                    isHighAux: 1, // true if chHigh are AUX6-9
                    ch4: 1;   // AUX1, included up here so ch0 starts on a byte boundary
            OTA_Channels_4x10 chLow;  // CH0-CH3
            OTA_Channels_4x10 chHigh; // AUX2-5 or AUX6-9
        } PACKED rc;
        struct {
            uint8_t packetType; // actually struct rc's first byte
            uint32_t packetNum; // LittleEndian
            uint8_t free[6];
        } PACKED dbg_linkstats;
        /** PACKET_TYPE_MSP **/
        struct {
            uint8_t packetType: 2,
                    packageIndex: 6;
            uint8_t payload[10];
        } msp_ul;
        /** PACKET_TYPE_SYNC **/
        struct {
            uint8_t packetType; // only low 2 bits
            OTA_Sync_s sync;
            uint8_t free[4];
        } PACKED sync;
        /** PACKET_TYPE_TLM **/
        struct {
            uint8_t packetType: 2,
                    containsLinkStats: 1,
                    packageIndex: 5;
            union {
                struct {
                    OTA_LinkStats_s stats;
                    uint8_t payload[10 - sizeof(OTA_LinkStats_s)];
                } PACKED ul_link_stats; // containsLinkStats == true
                uint8_t payload[10]; // containsLinkStats == false
            };
        } PACKED tlm_dl;
    };
    uint16_t crc;  // crc16 LittleEndian
} PACKED OTA_Packet8_s;

typedef struct {
    union {
        OTA_Packet4_s std;
        OTA_Packet8_s full;
    };
} PACKED OTA_Packet_s;

extern bool OtaIsFullRes;
extern volatile uint8_t OtaNonce;
extern uint16_t OtaCrcInitializer;
void OtaUpdateCrcInitFromUid();

enum OtaSwitchMode_e { smWideOr8ch = 0, smHybridOr16ch = 1, sm12ch = 2 };
void OtaUpdateSerializers(OtaSwitchMode_e const mode, expresslrs_RFrates_e const eRate);
extern OtaSwitchMode_e OtaSwitchModeCurrent;

// CRC
typedef std::function<bool (OTA_Packet_s * const otaPktPtr)> ValidatePacketCrc_t;
typedef std::function<void (OTA_Packet_s * const otaPktPtr)> GeneratePacketCrc_t;
extern ValidatePacketCrc_t OtaValidatePacketCrc;
extern GeneratePacketCrc_t OtaGeneratePacketCrc;

#if defined(TARGET_TX) || defined(UNIT_TEST)
typedef std::function<void (OTA_Packet_s * const otaPktPtr, CRSF const * const crsf, bool TelemetryStatus, uint8_t tlmDenom)> PackChannelData_t;
extern PackChannelData_t OtaPackChannelData;
#if defined(UNIT_TEST)
void OtaSetHybrid8NextSwitchIndex(uint8_t idx);
#endif
#endif

#if defined(TARGET_RX) || defined(UNIT_TEST)
typedef std::function<bool (OTA_Packet_s const * const otaPktPtr, CRSF * const crsf, uint8_t tlmDenom)> UnpackChannelData_t;
extern UnpackChannelData_t OtaUnpackChannelData;
#endif

#if defined(DEBUG_RCVR_LINKSTATS)
extern uint32_t debugRcvrLinkstatsPacketId;
#endif

#endif // H_OTA
