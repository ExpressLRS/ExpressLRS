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
typedef void (*GenerateChannelDataFunc)(volatile uint8_t* Buffer,
                                        volatile uint16_t* channels,
                                        volatile uint8_t* switches,
                                        bool TelemetryStatus);
#else
typedef void (*GenerateChannelDataFunc)(volatile uint8_t* Buffer,
                                        volatile uint16_t* channels,
                                        volatile uint8_t* switches);
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
                                              volatile uint8_t* switches,
                                              bool TelemetryStatus);
void ICACHE_RAM_ATTR GenerateChannelDataHybridSwitch8(
    volatile uint8_t* Buffer, volatile uint16_t* channels,
    volatile uint8_t* switches, bool TelemetryStatus);
#else
void ICACHE_RAM_ATTR GenerateChannelData10bit(volatile uint8_t* Buffer,
                                              volatile uint16_t* channels,
                                              volatile uint8_t* switches);
void ICACHE_RAM_ATTR GenerateChannelDataHybridSwitch8(
    volatile uint8_t* Buffer, volatile uint16_t* channels,
    volatile uint8_t* switches);
#endif

void ICACHE_RAM_ATTR UnpackChannelDataHybridSwitch8(volatile uint8_t* Buffer, CRSF *crsf);
void ICACHE_RAM_ATTR UnpackChannelData10bit(volatile uint8_t* Buffer, CRSF *crsf);

#endif

/**
 * Determine which switch to send next.
 * If any switch has changed since last sent, we send the lowest index changed switch
 * and set nextSwitchIndex to that value + 1.
 * If no switches have changed then we send nextSwitchIndex and increment the value.
 * For pure sequential switches, all 8 switches are part of the round-robin sequence.
 * For hybrid switches, switch 0 is sent with every packet and the rest of the switches
 * are in the round-robin.
 */
uint8_t ICACHE_RAM_ATTR getNextSwitchIndex();

#endif  // H_OTA
