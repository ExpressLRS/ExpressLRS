#ifndef H_OTA
#define H_OTA

#include "CRSF.h"

// expresslrs packet header types
// 00 -> standard 4 channel data packet
// 01 -> switch data packet
// 11 -> tlm packet
// 10 -> sync packet with hop data
#define RC_DATA_PACKET 0b00
#define MSP_DATA_PACKET 0b01
#define TLM_PACKET 0b11
#define SYNC_PACKET 0b10

#if TARGET_TX or defined UNIT_TEST
#ifdef ENABLE_TELEMETRY
void GenerateChannelData(volatile uint8_t* Buffer, CRSF *crsf, uint8_t addr, bool TelemetryStatus);
#else
void GenerateChannelData(volatile uint8_t* Buffer, CRSF *crsf, uint8_t addr);
#endif
#endif
#if TARGET_RX or defined UNIT_TEST
void UnpackChannelData(volatile uint8_t* Buffer, CRSF *crsf);
#endif

void ICACHE_RAM_ATTR GenerateMSPData(volatile uint8_t* Buffer, mspPacket_t *msp, uint8_t addr);
void ICACHE_RAM_ATTR UnpackMSPData(volatile uint8_t* Buffer, mspPacket_t *msp);

#endif // H_OTA
