#ifndef H_OTA
#define H_OTA

#include <functional>
#include "CRSF.h"
#include "telemetry_protocol.h"
#include <stddef.h>

#define OTA_PACKET_SIZE     8U
#define OTA_CRC_CALC_LEN    offsetof(OTA_Packet_s, crcLow)

// expresslrs packet header types
// 00 -> standard channel data packet
// 01 -> MSP data packet
// 11 -> TLM packet
// 10 -> sync packet
#define RC_DATA_PACKET 0b00
#define MSP_DATA_PACKET 0b01
#define TLM_PACKET 0b11
#define SYNC_PACKET 0b10

// Mask used to XOR the ModelId into the SYNC packet for ModelMatch
#define MODELMATCH_MASK 0x3f


typedef struct {
    uint8_t type: 2,
            crcHigh: 6;
    union {
        struct {
            uint8_t fhssIndex;
            uint8_t nonce;
            uint8_t switchEncMode:1,
                    newTlmRatio:3,
                    rateIndex:4;
            uint8_t UID3;
            uint8_t UID4;
            uint8_t UID5;
        } sync;
        struct {
            uint8_t ch0High;
            uint8_t ch1High;
            uint8_t ch2High;
            uint8_t ch3High;
            uint8_t ch3Low:2,
                    ch2Low:2,
                    ch1Low:2,
                    ch0Low:2;
            uint8_t ch4:1,
                    switches:7;
        } rc;
        struct {
            uint32_t packetNum;
            uint8_t free1;
            uint8_t free2;
        } PACKED dbg_linkstats;
        struct {
            uint8_t packageIndex;
            uint8_t payload[ELRS_TELEMETRY_BYTES_PER_CALL];
        } msp_ul;
        struct {
            uint8_t type:ELRS_TELEMETRY_SHIFT,
                    packageIndex:(8 - ELRS_TELEMETRY_SHIFT);
            union {
                struct {
                    uint8_t uplink_RSSI_1:7,
                            antenna:1;
                    uint8_t uplink_RSSI_2:7,
                            modelMatch:1;
                    uint8_t SNR;
                    uint8_t lq;
                    uint8_t mspConfirm:1,
                            free:7;
                } ul_link_stats;
                uint8_t payload[ELRS_TELEMETRY_BYTES_PER_CALL];
            };
        } tlm_dl;
    };
    uint8_t crcLow;
} PACKED OTA_Packet_s;


enum OtaSwitchMode_e { smHybrid = 0, smHybridWide = 1 };
void OtaSetSwitchMode(OtaSwitchMode_e mode);
extern OtaSwitchMode_e OtaSwitchModeCurrent;

#if defined(TARGET_TX) || defined(UNIT_TEST)
typedef std::function<void (OTA_Packet_s * const otaPktPtr, CRSF const * const crsf, bool TelemetryStatus, uint8_t nonce, uint8_t tlmDenom)> PackChannelData_t;
extern PackChannelData_t PackChannelData;
#if defined(UNIT_TEST)
void OtaSetHybrid8NextSwitchIndex(uint8_t idx);
#endif
#endif

#if defined(TARGET_RX) || defined(UNIT_TEST)
typedef std::function<bool (OTA_Packet_s const * const otaPktPtr, CRSF * const crsf, uint8_t nonce, uint8_t tlmDenom)> UnpackChannelData_t;
extern UnpackChannelData_t UnpackChannelData;
#endif

#if defined(DEBUG_RCVR_LINKSTATS)
extern uint32_t debugRcvrLinkstatsPacketId;
#endif

#endif // H_OTA
