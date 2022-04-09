#pragma once

#include "device.h"

#if defined(GPIO_PIN_BUTTON)
    #if (defined(TARGET_TX_BETAFPV_2400_V1) || defined(TARGET_TX_BETAFPV_900_V1) || defined(TARGET_TX_IFLIGHT)) || \
        (defined(TARGET_RX) && (defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)))
        extern device_t Button_device;
        #define HAS_BUTTON
    #endif
#endif
