#pragma once

#include "common.h"

#if defined(PLATFORM_ESP32)
#include "soc/soc_caps.h"
#endif

inline bool supportsSRXL2(uint8_t port, int8_t signalPin)
{
#if defined(TARGET_RX) && defined(PLATFORM_ESP32) && \
    (defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3))
    return port <= 1 && signalPin >= 0 && signalPin < 64 &&
        ((SOC_GPIO_VALID_OUTPUT_GPIO_MASK >> signalPin) & 1) &&
        (port != 0 || !OPT_CRSF_RCVR_NO_SERIAL);
#else
    return false;
#endif
}

inline bool supportsSRXL2() { return supportsSRXL2(0, GPIO_PIN_RCSIGNAL_TX); }

inline bool isValidSerialProtocolPair(uint8_t primary, uint8_t secondary)
{
#if defined(PLATFORM_ESP32)
    return primary <= PROTOCOL_SRXL2 && secondary <= PROTOCOL_SERIAL1_SRXL2 &&
        !(primary == PROTOCOL_SRXL2 && secondary == PROTOCOL_SERIAL1_SRXL2);
#else
    return primary <= PROTOCOL_SRXL2 && secondary == 0;
#endif
}
