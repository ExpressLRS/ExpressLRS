#pragma once

#include "common.h"
#include "device.h"
#include "devCRSF.h"
#if defined(USE_ANALOG_VBAT)
#include "devAnalogVbat.h"
#endif

#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)
    #define HAS_BARO
#endif

#if defined(HAS_BARO)

enum eBaroReadState : uint8_t
{
    brsNoBaro,
    brsUninitialized,
    brsReadTemp,
    brsWaitingTemp,
    brsReadPres,
    brsWaitingPress
};

extern device_t Baro_device;
#endif