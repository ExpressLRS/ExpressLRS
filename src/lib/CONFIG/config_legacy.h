#pragma once

/***
 * Outdated config structs used by the update process
 ***/

#include <inttypes.h>

/***
 * TX config
 ***/

// V5
typedef struct {
    uint8_t     rate:3;
    uint8_t     tlm:3;
    uint8_t     power:3;
    uint8_t     switchMode:2;
    uint8_t     modelMatch:1;
    uint8_t     dynamicPower:1;
    uint8_t     boostChannel:3;
} v5_model_config_t; // 16 bits

typedef struct {
    uint32_t        version;
    uint8_t         vtxBand;
    uint8_t         vtxChannel;
    uint8_t         vtxPower;
    uint8_t         vtxPitmode;
    uint8_t         powerFanThreshold:4; // Power level to enable fan if present
    v5_model_config_t  model_config[64];
    uint8_t         fanMode;
    uint8_t         motionMode;
} v5_tx_config_t;

// V6
typedef v5_model_config_t v6_model_config_t;

typedef struct {
    uint32_t        version;
    char            ssid[33];
    char            password[33];
    uint8_t         vtxBand;
    uint8_t         vtxChannel;
    uint8_t         vtxPower;
    uint8_t         vtxPitmode;
    uint8_t         powerFanThreshold:4; // Power level to enable fan if present
    v6_model_config_t  model_config[64];
    uint8_t         fanMode;
    uint8_t         motionMode;
    uint8_t         dvrAux:5;
    uint8_t         dvrStartDelay:3;
    uint8_t         dvrStopDelay:3;
} v6_tx_config_t;

// V7
typedef struct {
    uint32_t    rate:4,
                tlm:4,
                power:3,
                switchMode:2,
                boostChannel:3,
                dynamicPower:1,
                modelMatch:1,
                txAntenna:2,
                ptrStartChannel:4,
                ptrEnableChannel:5,
                linkMode:3;
} v7_model_config_t;

typedef struct {
    uint32_t        version;
    char            ssid[33];
    char            password[33];
    uint8_t         vtxBand;
    uint8_t         vtxChannel;
    uint8_t         vtxPower;
    uint8_t         vtxPitmode;
    uint8_t         powerFanThreshold:4; // Power level to enable fan if present
    v7_model_config_t  model_config[64];
    uint8_t         fanMode;
    uint8_t         motionMode;
    uint8_t         dvrAux:5;
    uint8_t         dvrStartDelay:3;
    uint8_t         dvrStopDelay:3;
} v7_tx_config_t;

/***
 * RX config
 ***/

// V4
typedef union {
    struct {
        uint16_t failsafe:10; // us output during failsafe +988 (e.g. 512 here would be 1500us)
        uint8_t inputChannel:4; // 0-based input channel
        uint8_t inverted:1; // invert channel output
        uint8_t unused:1;
    } val;
    uint16_t raw;
} v4_rx_config_pwm_t;

typedef struct {
    uint32_t    version;
    bool        isBound;
    uint8_t     uid[6];
    uint8_t     powerOnCounter;
    uint8_t     modelId;
    char        ssid[33];
    char        password[33];
    v4_rx_config_pwm_t pwmChannels[8];
} v4_rx_config_t;

// V5
typedef union {
    struct {
        uint32_t failsafe:10,    // us output during failsafe +988 (e.g. 512 here would be 1500us)
                 inputChannel:4, // 0-based input channel
                 inverted:1,     // invert channel output
                 mode:4,         // Output mode (eServoOutputMode)
                 narrow:1,       // Narrow output mode (half pulse width)
                 unused:13;      // FUTURE: When someone complains "everyone" uses inverted polarity PWM or something :/
    } val;
    uint32_t raw;
} v5_rx_config_pwm_t;

typedef struct {
    uint32_t    version;
    uint8_t     uid[6];             // Hardcoding in case UID_LEN changes.
    uint8_t     loanUID[6];         // Hardcoding in case UID_LEN changes.
    uint16_t    vbatScale;          // FUTURE: Override compiled vbat scale
    uint8_t     isBound:1,
                onLoan:1,
                power:4,
                antennaMode:2;      // 0=0, 1=1, 2=Diversity
    uint8_t     powerOnCounter:3,
                forceTlmOff:1,
                rateInitialIdx:4;   // Rate to start rateCycling at on boot
    uint8_t     modelId;
    v5_rx_config_pwm_t pwmChannels[16];
} v5_rx_config_t;

// V6
typedef union {
    struct {
        uint32_t failsafe:10,    // us output during failsafe +988 (e.g. 512 here would be 1500us)
                 inputChannel:4, // 0-based input channel
                 inverted:1,     // invert channel output
                 mode:4,         // Output mode (eServoOutputMode)
                 narrow:1,       // Narrow output mode (half pulse width)
                 unused:12;      // FUTURE: When someone complains "everyone" uses inverted polarity PWM or something :/
    } val;
    uint32_t raw;
} v6_rx_config_pwm_t;

typedef struct {
    uint32_t    version;
    uint8_t     uid[6];
    uint8_t     loanUID[6];
    uint16_t    vbatScale;          // FUTURE: Override compiled vbat scale
    uint8_t     isBound:1,
                onLoan:1,
                power:4,
                antennaMode:2;      // 0=0, 1=1, 2=Diversity
    uint8_t     powerOnCounter:3,
                forceTlmOff:1,
                rateInitialIdx:4;   // Rate to start rateCycling at on boot
    uint8_t     modelId;
    v6_rx_config_pwm_t pwmChannels[16];
} v6_rx_config_t;

typedef struct {
    uint32_t    version;
    uint8_t     uid[UID_LEN];
    uint8_t     loanUID[UID_LEN];
    uint16_t    vbatScale;          // FUTURE: Override compiled vbat scale
    uint8_t     isBound:1,
                onLoan:1,
                power:4,
                antennaMode:2;      // 0=0, 1=1, 2=Diversity
    uint8_t     powerOnCounter:3,
                forceTlmOff:1,
                rateInitialIdx:4;   // Rate to start rateCycling at on boot
    uint8_t     modelId;
    uint8_t     serialProtocol:4,
                failsafeMode:2,
                unused:2;
    v6_rx_config_pwm_t pwmChannels[16];
} v7_rx_config_t;

// V8 is just V7 except PWM config inserted 10khz PWM in the middle

typedef struct {
    uint32_t    version;
    uint8_t     uid[UID_LEN];
    uint8_t     unused_padding;
    uint8_t     serial1Protocol:4,  // secondary serial protocol
                serial1Protocol_unused:4;
    uint32_t    flash_discriminator;
    struct {
        uint16_t    scale;          // FUTURE: Override compiled vbat scale
        int16_t     offset;         // FUTURE: Override comiled vbat offset
    } vbat;
    uint8_t     bindStorage:2,     // rx_config_bindstorage_t
                power:4,
                antennaMode:2;      // 0=0, 1=1, 2=Diversity
    uint8_t     powerOnCounter:3,
                forceTlmOff:1,
                rateInitialIdx:4;   // Rate to start rateCycling at on boot
    uint8_t     modelId;
    uint8_t     serialProtocol:4,
                failsafeMode:2,
                unused:2;
    v6_rx_config_pwm_t pwmChannels[16];
    uint8_t     teamraceChannel:4,
                teamracePosition:3,
                teamracePitMode:1;  // FUTURE: Enable pit mode when disabling model
    uint8_t     targetSysId;
    uint8_t     sourceSysId;
} v9_rx_config_t;