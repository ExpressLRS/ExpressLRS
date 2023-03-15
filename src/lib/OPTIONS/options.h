#pragma once

#include "targets.h"

extern const unsigned char target_name[];
extern const uint8_t target_name_size;
extern const char commit[];
extern const char version[];

extern const char *wifi_hostname;
extern const char *wifi_ap_ssid;
extern const char *wifi_ap_password;
extern const char *wifi_ap_address;

enum BuzzerMode {
    buzzerQuiet,
    buzzerOne,
    buzzerTune
};

typedef struct _options {
    uint8_t     _magic_[8];     // this is the magic constant so the configurator can find this options block
    uint16_t    _version_;      // the version of this structure
    uint8_t     domain;         // depends on radio chip
    uint8_t     hasUID;
    uint8_t     uid[6];         // MY_UID derived from MY_BINDING_PHRASE
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    int32_t     wifi_auto_on_interval;
    char        home_wifi_ssid[33];
    char        home_wifi_password[65];
#endif
#if defined(TARGET_RX)
    uint32_t    uart_baud;
    bool        _unused1:1; // invert_tx
    bool        lock_on_first_connection:1;
    bool        _unused2:1; // r9mm_mini_sbus
    bool        is_airport:1;
#endif
#if defined(TARGET_TX)
    uint32_t    tlm_report_interval;
    uint32_t    fan_min_runtime;
    bool        uart_inverted:1;
    bool        unlock_higher_power:1;
    bool        is_airport:1;
#if defined(GPIO_PIN_BUZZER)
    uint8_t     buzzer_mode;            // 0 = disable all, 1 = beep once, 2 = disable startup beep, 3 = default tune, 4 = custom tune
    uint16_t    buzzer_melody[32][2];
#endif
    uint32_t    uart_baud;              // only use for airport
#endif
} __attribute__((packed)) firmware_options_t;

// Layout is PRODUCTNAME DEVICENAME OPTIONS HARDWARE
constexpr size_t ELRSOPTS_PRODUCTNAME_SIZE = 128;
constexpr size_t ELRSOPTS_DEVICENAME_SIZE = 16;
constexpr size_t ELRSOPTS_OPTIONS_SIZE = 512;
constexpr size_t ELRSOPTS_HARDWARE_SIZE = 2048;

#if defined(TARGET_UNIFIED_TX) || defined(TARGET_UNIFIED_RX)
extern firmware_options_t firmwareOptions;
extern char product_name[];
extern char device_name[];
extern bool options_init();
extern String& getOptions();
extern String& getHardware();
extern void saveOptions();

#include "EspFlashStream.h"
extern bool options_HasStringInFlash(EspFlashStream &strmFlash);
#else
extern const firmware_options_t firmwareOptions;
extern const char device_name[];
extern const char *product_name;
#endif
