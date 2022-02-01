#pragma once

extern const unsigned char target_name[];
extern const uint8_t target_name_size;
extern const char device_name[];
extern const uint8_t device_name_size;
extern const char commit[];
extern const char version[];
extern const char PROGMEM compile_options[];

extern const char *wifi_hostname;
extern const char *wifi_ap_ssid;
extern const char *wifi_ap_password;
extern const char *wifi_ap_address;

typedef struct _options {
    uint8_t     _magic_[8];     // this is the magic constant so the configurator can find this options block
    uint16_t    _version_;      // the version of this structure
    uint8_t     _hardware_;     // The hardware type (ESP|STM, TX|RX, BUZZER|NONE)
    uint8_t     hasUID:1;
    uint8_t     uid[6];         // MY_UID derived from MY_BINDING_PHRASE
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    int         wifi_auto_on_interval;
    char        home_wifi_ssid[33];
    char        home_wifi_password[65];
#endif
#if defined(TARGET_RX)
    uint32_t    uart_baud;
    bool        invert_tx:1;
    bool        lock_on_first_connection:1;
#endif
#if defined(TARGET_TX)
    uint32_t    tlm_report_interval;
    bool        no_sync_on_arm:1;
    bool        uart_inverted:1;
    bool        unlock_higher_power:1;
#if defined(GPIO_PIN_BUZZER) && GPIO_PIN_BUZZER != UNDEF_PIN
    uint8_t     buzzer_mode;            // 0 = disable all, 1 = beep once, 2 = disable startup beep, 3 = default tune, 4 = custom tune
    uint16_t    buzzer_melody[32][2];
#endif
#endif
} __attribute__((packed)) firmware_options_t;

extern const firmware_options_t firmwareOptions;