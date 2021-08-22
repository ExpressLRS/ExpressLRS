#ifndef H_OTA
#define H_OTA

#include <functional>
#include "CRSF.h"

// expresslrs packet header types
// 00 -> standard channel data packet
// 01 -> MSP data packet
// 11 -> TLM packet
// 10 -> sync packet
#define RC_DATA_PACKET 0b00
#define MSP_DATA_PACKET 0b01
#define TLM_PACKET 0b11
#define SYNC_PACKET 0b10

enum OtaSwitchMode_e { smHybrid, smHybridWide };
void OtaSetSwitchMode(OtaSwitchMode_e mode);
extern OtaSwitchMode_e OtaSwitchModeCurrent;

#if defined(TARGET_TX) || defined(UNIT_TEST)
typedef std::function<void (volatile uint8_t* Buffer, CRSF *crsf, bool TelemetryStatus, uint8_t nonce, uint8_t tlmDenom)> PackChannelData_t;
extern PackChannelData_t PackChannelData;
#if defined(UNIT_TEST)
void OtaSetHybrid8NextSwitchIndex(uint8_t idx);
#endif
#endif

#if defined(TARGET_RX) || defined(UNIT_TEST)
typedef std::function<bool (volatile uint8_t* Buffer, CRSF *crsf, uint8_t nonce, uint8_t tlmDenom)> UnpackChannelData_t;
extern UnpackChannelData_t UnpackChannelData;
#endif

#endif // H_OTA
