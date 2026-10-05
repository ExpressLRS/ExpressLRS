#pragma once

#include "common.h"

inline bool supportsSRXL2()
{
#if defined(TARGET_RX) && defined(PLATFORM_ESP32) && \
    (defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3))
    return !OPT_CRSF_RCVR_NO_SERIAL && GPIO_PIN_RCSIGNAL_TX != UNDEF_PIN;
#else
    return false;
#endif
}
