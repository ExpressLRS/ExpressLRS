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

#if defined HYBRID_SWITCHES_8 or defined UNIT_TEST

#if TARGET_TX or defined UNIT_TEST
void GenerateChannelDataHybridSwitch8(volatile uint8_t* Buffer, CRSF *crsf, uint8_t addr, bool TelemetryStatus);
#elif TARGET_RX or defined UNIT_TEST
void UnpackChannelDataHybridSwitches8(volatile uint8_t* Buffer, CRSF *crsf);
#endif

#endif // HYBRID_SWITCHES_8

#if defined SEQ_SWITCHES or defined UNIT_TEST

#if TARGET_TX or defined UNIT_TEST
void ICACHE_RAM_ATTR GenerateChannelDataSeqSwitch(volatile uint8_t* Buffer, CRSF *crsf, uint8_t addr);
#elif TARGET_RX or defined UNIT_TEST
void UnpackChannelDataSeqSwitches(volatile uint8_t* Buffer, CRSF *crsf);
#endif

#endif // SEQ_SWITCHES

#if (!defined HYBRID_SWITCHES_8 and !defined SEQ_SWITCHES) or defined UNIT_TEST

#if TARGET_TX or defined UNIT_TEST
void ICACHE_RAM_ATTR Generate4ChannelData_10bit(volatile uint8_t* Buffer, CRSF *crsf, uint8_t addr);
#elif TARGET_RX or defined UNIT_TEST
void ICACHE_RAM_ATTR UnpackChannelData_10bit(volatile uint8_t* Buffer, CRSF *crsf);
#endif

#endif // !HYBRID_SWITCHES_8 and !SEQ_SWITCHES

void ICACHE_RAM_ATTR GenerateMSPData(volatile uint8_t* Buffer, mspPacket_t *msp, uint8_t addr);
void ICACHE_RAM_ATTR UnpackMSPData(volatile uint8_t* Buffer, mspPacket_t *msp);

#endif // H_OTA
