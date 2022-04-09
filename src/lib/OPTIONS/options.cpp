#include "targets.h"
#include "options.h"

#define QUOTE(arg) #arg
#define STR(macro) QUOTE(macro)
const unsigned char target_name[] = "\xBE\xEF\xCA\xFE" STR(TARGET_NAME);
const uint8_t target_name_size = sizeof(target_name);
const char commit[] {LATEST_COMMIT, 0};
const char version[] = {LATEST_VERSION, 0};

#if defined(TARGET_TX)
const char *wifi_hostname = "elrs_tx";
const char *wifi_ap_ssid = "ExpressLRS TX";
#else
const char *wifi_hostname = "elrs_rx";
const char *wifi_ap_ssid = "ExpressLRS RX";
#endif
const char *wifi_ap_password = "expresslrs";
const char *wifi_ap_address = "10.0.0.1";

__attribute__ ((used)) const firmware_options_t firmwareOptions = {
    ._magic_ = {0xBE, 0xEF, 0xBA, 0xBE, 0xCA, 0xFE, 0xF0, 0x0D},
    ._version_ = 0,
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    ._hasWiFi = 1,
#else
    ._hasWiFi = 0,
#endif
#if defined(GPIO_PIN_BUZZER)
    ._hasBuzzer = 1,
#else
    ._hasBuzzer = 0,
#endif
#if defined(PLATFORM_STM32) || defined(UNIT_TEST)
    ._mcu_type = 0,
#elif defined(PLATFORM_ESP32)
    ._mcu_type = 1,
#elif defined(PLATFORM_ESP8266)
    ._mcu_type = 2,
#else
    #error Unsupported MCU type
#endif
#if defined(TARGET_TX) || defined(UNIT_TEST)
    ._device_type = 0,
#elif defined(TARGET_RX)
    ._device_type = 1,
#else
    #error Unsupported device type
#endif
#if defined(Regulatory_Domain_ISM_2400)
    ._radio_chip = 1,
#else
    ._radio_chip = 0,
#endif
#if defined(MY_UID)
    .hasUID = true,
    .uid = { MY_UID },
#else
    .hasUID = false,
    .uid = {},
#endif
#if defined(DEVICE_NAME_ARR)
    .device_name = {DEVICE_NAME_ARR},
#else
    .device_name = DEVICE_NAME,
#endif
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    #if defined(AUTO_WIFI_ON_INTERVAL)
        .wifi_auto_on_interval = AUTO_WIFI_ON_INTERVAL * 1000,
    #else
        .wifi_auto_on_interval = -1,
    #endif
    #if defined(HOME_WIFI_SSID)
        .home_wifi_ssid = {HOME_WIFI_SSID},
    #else
        .home_wifi_ssid = {},
    #endif
    #if defined(HOME_WIFI_PASSWORD)
        .home_wifi_password = {HOME_WIFI_PASSWORD},
    #else
        .home_wifi_password = {},
    #endif
#endif
#if defined(TARGET_RX)
#if defined(RCVR_UART_BAUD)
    .uart_baud = RCVR_UART_BAUD,
#else
    .uart_baud = 420000,
#endif
#if defined(RCVR_INVERT_TX)
    .invert_tx = true,
#else
    .invert_tx = false,
#endif
#if defined(LOCK_ON_FIRST_CONNECTION)
    .lock_on_first_connection = true,
#else
    .lock_on_first_connection = false,
#endif
#if defined(USE_R9MM_R9MINI_SBUS)
    .r9mm_mini_sbus = true,
#else
    .r9mm_mini_sbus = false,
#endif
#endif
#if defined(TARGET_TX)
#if defined(TLM_REPORT_INTERVAL_MS)
    .tlm_report_interval = TLM_REPORT_INTERVAL_MS,
#else
    .tlm_report_interval = 320U,
#endif
#if defined(FAN_MIN_RUNTIME)
    .fan_min_runtime = FAN_MIN_RUNTIME,
#else
    .fan_min_runtime = 30,
#endif
#if defined(NO_SYNC_ON_ARM)
    .no_sync_on_arm = true,
#else
    .no_sync_on_arm = false,
#endif
#if defined(UART_INVERTED) // Only on ESP32
    .uart_inverted = true,
#else
    .uart_inverted = false,
#endif
#if defined(UNLOCK_HIGHER_POWER)
    .unlock_higher_power = true,
#else
    .unlock_higher_power = false,
#endif
#if defined(GPIO_PIN_BUZZER)
    #if defined(DISABLE_ALL_BEEPS)
    .buzzer_mode = buzzerQuiet,
    .buzzer_melody = {},
    #elif defined(JUST_BEEP_ONCE)
    .buzzer_mode = buzzerOne,
    .buzzer_melody = {},
    #elif defined(DISABLE_STARTUP_BEEP)
    .buzzer_mode = buzzerTune,
    .buzzer_melody = {{400, 200}, {480, 200}},
    #elif defined(MY_STARTUP_MELODY)
    .buzzer_mode = buzzerTune,
    .buzzer_melody = MY_STARTUP_MELODY_ARR,
    #else
    .buzzer_mode = buzzerTune,
    .buzzer_melody = {{659, 300}, {659, 300}, {523, 100}, {659, 300}, {783, 550}, {392, 575}},
    #endif
#endif
#endif
};

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
