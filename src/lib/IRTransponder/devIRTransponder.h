//
// Authors: 
// * Mickey (mha1, initial RMT implementation)
// * Dominic Clifton (hydra, refactoring for multiple-transponder systems, iLap support)
//

#pragma once

#include "common.h"
#include "device.h"

#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)

extern device_t ir_transponder_device;

#endif