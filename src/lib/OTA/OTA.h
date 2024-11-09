#ifndef H_OTA
#define H_OTA

#include <cstddef>

#include "crc.h"
#include "CRSF.h"
#include "crsf_protocol.h"
#include "telemetry_protocol.h"
#include "FIFO.h"

#define OTA4_PACKET_SIZE     8U
#define OTA4_CRC_CALC_LEN    offsetof(OTA_Packet4_s, crcLow)
#define OTA8_PACKET_SIZE     13U
#define OTA8_CRC_CALC_LEN    offsetof(OTA_Packet8_s, crc)

// Packet header types
#define PACKET_TYPE_DATA        0b01
// Uplink only header types
#define PACKET_TYPE_RCDATA      0b00
#define PACKET_TYPE_SYNC        0b10
// Downlink only header types
#define PACKET_TYPE_LINKSTATS   0b00

// Mask used to XOR the ModelId into the SYNC packet for ModelMatch
#define MODELMATCH_MASK 0x3f

typedef struct {
    uint8_t fhssIndex;
    uint8_t nonce;
    uint8_t rfRateEnum;
    uint8_t switchEncMode:1,
            newTlmRatio:3,
            free:4;
    uint8_t UID4;
    uint8_t UID5;
} PACKED OTA_Sync_s;

typedef struct {
    uint8_t uplink_RSSI_1:7,
            antenna:1;
    uint8_t uplink_RSSI_2:7,
            modelMatch:1;
    uint8_t lq:7,
            tlmConfirm:1;
    int8_t SNR;
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
            uint8_t packageIndex:7,
                    tlmConfirm:1;
            uint8_t payload[ELRS4_MSP_BYTES_PER_CALL];
        } msp_ul;
        /** PACKET_TYPE_SYNC **/
        OTA_Sync_s sync;
        /** PACKET_TYPE_TLM **/
        struct {
            uint8_t free:1,
                    tlmConfirm: 1,
                    packageIndex:(8 - ELRS4_TELEMETRY_SHIFT);
            union {
                struct {
                    OTA_LinkStats_s stats;
                    uint8_t free;
                } PACKED ul_link_stats;
                uint8_t payload[ELRS4_TELEMETRY_BYTES_PER_CALL];
            };
        } tlm_dl; // PACKET_TYPE_TLM
        /** PACKET_TYPE_AIRPORT **/
        struct {
            uint8_t free:2,
                    count:(8 - ELRS4_TELEMETRY_SHIFT);
            uint8_t payload[ELRS4_TELEMETRY_BYTES_PER_CALL];
        } PACKED airport;
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
                    uplinkPower: 3, // CRSF_power_level - 1 (1-8 is 0-7 in the air)
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
                    packageIndex: 5,
                    tlmConfirm: 1;
            uint8_t payload[ELRS8_MSP_BYTES_PER_CALL];
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
                    tlmConfirm: 1,
                    packageIndex: 5;
            union {
                struct {
                    OTA_LinkStats_s stats;
                    uint8_t payload[ELRS8_TELEMETRY_BYTES_PER_CALL - sizeof(OTA_LinkStats_s)];
                } PACKED ul_link_stats; // containsLinkStats == true
                uint8_t payload[ELRS8_TELEMETRY_BYTES_PER_CALL]; // containsLinkStats == false
            };
        } PACKED tlm_dl;
        /** PACKET_TYPE_AIRPORT **/
        struct {
            uint8_t packetType: 2,
                    free: 1,
                    count: 5;
            uint8_t payload[ELRS8_TELEMETRY_BYTES_PER_CALL];
        } PACKED airport;
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
void OtaUpdateSerializers(OtaSwitchMode_e const mode, uint8_t packetSize);
extern OtaSwitchMode_e OtaSwitchModeCurrent;

// CRC
typedef bool (*ValidatePacketCrc_t)(OTA_Packet_s * const otaPktPtr);
typedef void (*GeneratePacketCrc_t)(OTA_Packet_s * const otaPktPtr);
extern ValidatePacketCrc_t OtaValidatePacketCrc;
extern GeneratePacketCrc_t OtaGeneratePacketCrc;
// Value is implicit leading 1, comment is Koopman formatting (implicit trailing 1) https://users.ece.cmu.edu/~koopman/crc/
#define ELRS_CRC_POLY 0x07 // 0x83
#define ELRS_CRC14_POLY 0x2E57 // 0x372b
#define ELRS_CRC16_POLY 0x3D65 // 0x9eb2

#if defined(TARGET_TX) || defined(UNIT_TEST)
typedef void (*PackChannelData_t)(OTA_Packet_s * const otaPktPtr, const uint32_t *channelData, bool TelemetryStatus, uint8_t tlmDenom);
extern PackChannelData_t OtaPackChannelData;
#if defined(UNIT_TEST)
void OtaSetHybrid8NextSwitchIndex(uint8_t idx);
void OtaSetFullResNextChannelSet(bool next);
#endif
#endif

#if defined(TARGET_RX) || defined(UNIT_TEST)
typedef bool (*UnpackChannelData_t)(OTA_Packet_s const * const otaPktPtr, uint32_t *channelData, uint8_t tlmDenom);
extern UnpackChannelData_t OtaUnpackChannelData;
#endif

void OtaPackAirportData(OTA_Packet_s * const otaPktPtr, FIFO<AP_MAX_BUF_LEN> *inputBuffer);
void OtaUnpackAirportData(OTA_Packet_s const * const otaPktPtr, FIFO<AP_MAX_BUF_LEN> *outputBuffer);

#if defined(DEBUG_RCVR_LINKSTATS)
extern uint32_t debugRcvrLinkstatsPacketId;
#endif

#endif // H_OTA
