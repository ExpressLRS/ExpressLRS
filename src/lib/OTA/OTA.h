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


void GenerateChannelDataAnalog7(volatile uint8_t* Buffer, CRSF *crsf, uint8_t addr);
void UnpackChannelDataAnalog7(volatile uint8_t* Buffer, CRSF *crsf);
void GenerateChannelDataHybridSwitch8(volatile uint8_t* Buffer, CRSF *crsf, uint8_t addr);
void UnpackChannelDataHybridSwitches8(volatile uint8_t* Buffer, CRSF *crsf);


void ICACHE_RAM_ATTR GenerateChannelDataSeqSwitch(volatile uint8_t* Buffer, CRSF *crsf, uint8_t addr);
void UnpackChannelDataSeqSwitches(volatile uint8_t* Buffer, CRSF *crsf);



#endif // H_OTA
