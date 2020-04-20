#ifndef H_OTA
#define H_OTA

#include "LoRaRadioLib.h" // this has to come before CRSF.h when compiling on R9
#include "CRSF.h"

// expresslrs packet header types
// 00 -> standard 4 channel data packet
// 01 -> switch data packet
// 11 -> tlm packet
// 10 -> sync packet with hop data
#define RC_DATA_PACKET 0b00
#define SWITCH_DATA_PACKET 0b01
#define TLM_PACKET 0b11
#define SYNC_PACKET 0b10

#if defined HYBRID_SWITCHES_8 or defined UNIT_TEST

void GenerateChannelDataHybridSwitch8(SX127xDriver *Radio, CRSF *crsf, uint8_t addr);
void UnpackChannelDataHybridSwitches8(SX127xDriver *Radio, CRSF *crsf);

#endif // HYBRID_SWITCHES_8

#if defined SEQ_SWITCHES or defined UNIT_TEST

void ICACHE_RAM_ATTR GenerateChannelDataSeqSwitch(SX127xDriver *Radio, CRSF *crsf, uint8_t addr);
void UnpackChannelDataSeqSwitches(SX127xDriver *Radio, CRSF *crsf);

#endif // SEQ_SWITCHES


#endif // H_OTA
