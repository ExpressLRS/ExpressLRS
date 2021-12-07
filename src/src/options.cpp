#include "targets.h"
#include "options.h"

#define QUOTE(arg) #arg
#define STR(macro) QUOTE(macro)
const unsigned char target_name[] = "\xBE\xEF\xCA\xFE" STR(TARGET_NAME);
const uint8_t target_name_size = sizeof(target_name);
const char device_name[] = STR(DEVICE_NAME);
const uint8_t device_name_size = sizeof(device_name);

const char PROGMEM compile_options[] = {
#ifdef MY_BINDING_PHRASE
    "-DMY_BINDING_PHRASE=\"" STR(MY_BINDING_PHRASE) "\" "
#endif

#ifdef TARGET_TX
    #ifdef UNLOCK_HIGHER_POWER
        "-DUNLOCK_HIGHER_POWER "
    #endif
    #ifdef NO_SYNC_ON_ARM
        "-DNO_SYNC_ON_ARM "
    #endif
    #ifdef FEATURE_OPENTX_SYNC
        "-DFEATURE_OPENTX_SYNC "
    #endif
    #ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
        "-DFEATURE_OPENTX_SYNC_AUTOTUNE "
    #endif
    #ifdef UART_INVERTED
        "-DUART_INVERTED "
    #endif
    #ifdef DISABLE_ALL_BEEPS
        "-DDISABLE_ALL_BEEPS "
    #endif
    #ifdef JUST_BEEP_ONCE
        "-DJUST_BEEP_ONCE "
    #endif
    #ifdef DISABLE_STARTUP_BEEP
        "-DDISABLE_STARTUP_BEEP "
    #endif
    #ifdef MY_STARTUP_MELODY
        "-DMY_STARTUP_MELODY=\"" STR(MY_STARTUP_MELODY) "\" "
    #endif
    #ifdef WS2812_IS_GRB
        "-DWS2812_IS_GRB "
    #endif
    #ifdef TLM_REPORT_INTERVAL_MS
        "-DTLM_REPORT_INTERVAL_MS=" STR(TLM_REPORT_INTERVAL_MS) " "
    #endif
    #ifdef USE_TX_BACKPACK
        "-DUSE_TX_BACKPACK "
    #endif
    #ifdef USE_BLE_JOYSTICK
        "-DUSE_BLE_JOYSTICK "
    #endif
#endif

#ifdef TARGET_RX
    #ifdef LOCK_ON_FIRST_CONNECTION
        "-DLOCK_ON_FIRST_CONNECTION "
    #endif
    #ifdef USE_R9MM_R9MINI_SBUS
        "-DUSE_R9MM_R9MINI_SBUS "
    #endif
    #ifdef AUTO_WIFI_ON_INTERVAL
        "-DAUTO_WIFI_ON_INTERVAL=" STR(AUTO_WIFI_ON_INTERVAL) " "
    #endif
    #ifdef USE_DIVERSITY
        "-DUSE_DIVERSITY "
    #endif
    #ifdef RCVR_UART_BAUD
        "-DRCVR_UART_BAUD=" STR(RCVR_UART_BAUD) " "
    #endif
    #ifdef RCVR_INVERT_TX
        "-DRCVR_INVERT_TX "
    #endif
    #ifdef USE_R9MM_R9MINI_SBUS
        "-DUSE_R9MM_R9MINI_SBUS "
    #endif
#endif
};
