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
    uint32_t    flash_discriminator;    // Discriminator value used to determine if the device has been reflashed and therefore
                                        // the SPIFSS settings are obsolete and the flashed settings should be used in preference
    uint32_t    fan_min_runtime;
    int32_t     wifi_auto_on_interval;
    char        home_wifi_ssid[33];
    char        home_wifi_password[65];
#if defined(TARGET_RX)
    uint32_t    uart_baud;
    bool        _unused1:1; // invert_tx
    bool        lock_on_first_connection:1;
    bool        dji_permanently_armed:1;
    bool        is_airport:1;
#endif
#if defined(TARGET_TX) || defined(UNIT_TEST)
    uint32_t    tlm_report_interval;
    bool        _unused1:1;
    bool        unlock_higher_power:1;
    bool        is_airport:1;
    uint32_t    uart_baud;              // only use for airport
#endif
} __attribute__((packed)) firmware_options_t;

// Layout is PRODUCTNAME DEVICENAME OPTIONS HARDWARE
constexpr size_t ELRSOPTS_PRODUCTNAME_SIZE = 128;
constexpr size_t ELRSOPTS_DEVICENAME_SIZE = 16;
constexpr size_t ELRSOPTS_OPTIONS_SIZE = 512;
constexpr size_t ELRSOPTS_HARDWARE_SIZE = 2048;

extern firmware_options_t firmwareOptions;
extern bool options_init();

#if defined(UNIT_TEST)
extern char *product_name;
extern char *device_name;
#else
extern char product_name[];
extern char device_name[];
extern uint32_t logo_image;
extern String& getOptions();
extern String& getHardware();
extern void saveOptions();

#include "EspFlashStream.h"
bool options_HasStringInFlash(EspFlashStream &strmFlash);
void options_SetTrueDefaults();
#endif
