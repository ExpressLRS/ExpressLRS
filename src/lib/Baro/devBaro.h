#pragma once

#include "common.h"
#include "device.h"
#include "devAnalogVbat.h"

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