#include "targets.h"
#include "options.h"

#define QUOTE(arg) #arg
#define STR(macro) QUOTE(macro)
const unsigned char target_name[] = "\xBE\xEF\xCA\xFE" STR(TARGET_NAME);
const uint8_t target_name_size = sizeof(target_name);

#if defined(TARGET_TX)
const char *wifi_hostname = "elrs_tx";
const char *wifi_ap_ssid = "ExpressLRS TX";
#else
const char *wifi_hostname = "elrs_rx";
const char *wifi_ap_ssid = "ExpressLRS RX";
#endif
const char *wifi_ap_password = "expresslrs";
const char *wifi_ap_address = "10.0.0.1";

const char *home_wifi_ssid = ""
#ifdef HOME_WIFI_SSID
STR(HOME_WIFI_SSID)
#endif
;
const char *home_wifi_password = ""
#ifdef HOME_WIFI_PASSWORD
STR(HOME_WIFI_PASSWORD)
#endif
;

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
#endif
};
