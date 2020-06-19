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

void GenerateChannelDataHybridSwitch8(uint8_t *Buffer, CRSF *crsf, uint8_t addr);
void UnpackChannelDataHybridSwitches8(uint8_t *Buffer, CRSF *crsf);

#endif // HYBRID_SWITCHES_8

#if defined SEQ_SWITCHES or defined UNIT_TEST

void ICACHE_RAM_ATTR GenerateChannelDataSeqSwitch(uint8_t *Buffer, CRSF *crsf, uint8_t addr);
void UnpackChannelDataSeqSwitches(uint8_t *Buffer, CRSF *crsf);

#endif // SEQ_SWITCHES


#endif // H_OTA
