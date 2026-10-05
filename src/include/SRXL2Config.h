#pragma once

#include "common.h"

inline bool supportsSRXL2()
{
#if defined(TARGET_RX) && defined(PLATFORM_ESP32) && !defined(PLATFORM_ESP32_S3) && !defined(PLATFORM_ESP32_C3)
    return !OPT_CRSF_RCVR_NO_SERIAL && GPIO_PIN_RCSIGNAL_TX != UNDEF_PIN;
#else
    return false;
#endif
}

inline bool serialProtocolSupported(uint8_t protocol, bool smartSupported)
{
    return protocol <= PROTOCOL_SCORPION_TLM || (protocol == PROTOCOL_SRXL2 && smartSupported);
}
