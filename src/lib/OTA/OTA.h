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

// Define GenerateChannelData() function pointer
#ifdef ENABLE_TELEMETRY
typedef void (*GenerateChannelDataFunc)(
    volatile uint8_t* Buffer, volatile uint16_t* channels, CRSF* crsf,
    bool TelemetryStatus);
#else
typedef void (*GenerateChannelDataFunc)(
    volatile uint8_t* Buffer, volatile uint16_t* channels, CRSF* crsf);
#endif

typedef void (*UnpackChannelDataFunc)(
    volatile uint8_t* Buffer, CRSF* crsf);

#if TARGET_TX or defined UNIT_TEST
extern GenerateChannelDataFunc GenerateChannelData;
#endif

#if TARGET_RX or defined UNIT_TEST
extern UnpackChannelDataFunc UnpackChannelData;
#endif

// This could be called again if the config changes, for instance
void OTAInitMethods();

#if defined(UNIT_TEST)

#ifdef ENABLE_TELEMETRY
void ICACHE_RAM_ATTR GenerateChannelData10bit(volatile uint8_t* Buffer,
                                              volatile uint16_t* channels,
                                              CRSF* crsf, bool TelemetryStatus);
void ICACHE_RAM_ATTR GenerateChannelDataHybridSwitch8(
    volatile uint8_t* Buffer, volatile uint16_t* channels, CRSF* crsf,
    bool TelemetryStatus);
#else
void ICACHE_RAM_ATTR GenerateChannelData10bit(volatile uint8_t* Buffer,
                                              volatile uint16_t* channels,
                                              CRSF* crsf);
void ICACHE_RAM_ATTR GenerateChannelDataHybridSwitch8(
    volatile uint8_t* Buffer, volatile uint16_t* channels, CRSF* crsf);
#endif

void ICACHE_RAM_ATTR UnpackChannelDataHybridSwitch8(volatile uint8_t* Buffer, CRSF *crsf);
void ICACHE_RAM_ATTR UnpackChannelData10bit(volatile uint8_t* Buffer, CRSF *crsf);

#endif

#endif  // H_OTA
