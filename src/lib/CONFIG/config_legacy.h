#pragma once

/***
 * Outdated config structs used by the update process
 *
 * TX V7 Jun 22 2022 (3.0.0)
 * TX V8 (4.0.0)
 *
 * RX V5 Jun 22 2022 (3.0.0)
 * RX V6 Nov 09 2022 (3.1.0)
 * RX V7 Mar 13 2023 (3.3.0)
 * RX V8 Sep 29 2023 (3.4.0?)
 * RX V9 Feb 16 2024 (3.4.0)
 * RX V10 (4.0.0)
 * RX V11 (4.0.0)
 *
 ***/

#include <inttypes.h>
#include <stddef.h>

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

// V8 = V7 upgradable to tx_config_t (current) - tx_config_t without minPower,
// but with same layout for all fields except byte at offset 9 (padding -> minPower).
// model_config_t here is the current TX model struct (post V7->V8 conversion).
// Used only for the V8 -> V9 EEPROM upgrade path on ESP8266.
//
// NOTE: interim pre-release builds on this branch (d13aafbe and
// descendants) stored a real minPower floor in this very padding byte
// while still labelling the image TX_CONFIG_VERSION 8. On upgrade,
// UpgradeEepromV8ToV9 does NOT preserve it: genuine-v8 garbage and an
// interim floor are indistinguishable, and resurrecting garbage could
// leave Dynamic Power at an unintended high minimum. The floor is reset
// to getMinPower(); interim users simply re-set it once. See the comment
// in UpgradeEepromV8ToV9.
typedef struct {
    uint32_t        version;
    uint8_t         vtxBand;
    uint8_t         vtxChannel;
    uint8_t         vtxPower;
    uint8_t         vtxPitmode;
    uint8_t         powerFanThreshold:4;
    // offset 9 in V8 is 3 bytes of struct padding (byte 9, 10, 11) before model_config at offset 12.
    // In V9 byte 9 becomes minPower; bytes 10-11 remain padding. Byte 9 is
    // never read as a saved floor during upgrade (undefined padding garbage
    // in genuine v8; interim-build floors intentionally discarded — see
    // NOTE above and UpgradeEepromV8ToV9).
    uint8_t         _pad9_11[3];
    struct {
        uint32_t    rate:5, tlm:4, power:3, switchMode:2, boostChannel:3,
                    dynamicPower:1, modelMatch:1, txAntenna:2,
                    ptrStartChannel:4, ptrEnableChannel:5, linkMode:2;
    } model_config[64];
    uint8_t         fanMode;
    uint8_t         motionMode:2, dvrStopDelay:3, backpackDisable:1, backpackTlmMode:2;
    uint8_t         dvrStartDelay:3, dvrAux:5;
    // Explicit padding to match the 4-byte alignment of tx_button_color_t
    // (union containing uint32 raw) in tx_config_t: buttonColors must start at
    // offset 272, not 271. Without this, field offsets would be wrong.
    uint8_t         _pad271;
    union {
        struct {
            uint8_t     color;
            struct { uint8_t pressType:1, count:3, action:4; } actions[2];
            uint8_t     unused;
        } val;
        uint32_t raw;
    } buttonColors[2];
} v8_tx_config_t;

// Layout assertions: v8 and v9 must be byte-identical apart from byte 9 (see
// comment in UpgradeEepromV8ToV9 and the note on v8_tx_config_t). These
// offsetof checks run at compile time on the build host compiler; they
// verify the intentional layout assumption that the migration depends on.
// If a future field edit moves offsets, these will fail loudly instead of
// silently corrupting the EEPROM image on upgrade.
//
// model_config_t itself is defined in config.h, not here, so offsetof on
// tx_config_t's model_config is checked where both types are visible
// (config.cpp, in UpgradeEepromV8ToV9). Here we assert the legacy-struct
// internal offsets only.
static_assert(offsetof(v8_tx_config_t, version) == 0, "v8 version offset");
static_assert(offsetof(v8_tx_config_t, _pad9_11) == 9, "v8 pad offset (minPower reuse point)");
static_assert(offsetof(v8_tx_config_t, model_config) == 12, "v8 model_config offset");
static_assert(offsetof(v8_tx_config_t, fanMode) == 268, "v8 fanMode offset");
static_assert(offsetof(v8_tx_config_t, _pad271) == 271, "v8 pad271 offset");
static_assert(offsetof(v8_tx_config_t, buttonColors) == 272, "v8 buttonColors offset");

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
typedef v6_rx_config_pwm_t v8_rx_config_pwm_t;

// failsafeMode was added during v8, no new version number created, but was in v9
typedef union {
    struct {
        uint32_t failsafe:10,    // us output during failsafe +988 (e.g. 512 here would be 1500us)
                 inputChannel:4, // 0-based input channel
                 inverted:1,     // invert channel output
                 mode:4,         // Output mode (eServoOutputMode)
                 narrow:1,       // Narrow output mode (half pulse width)
                 failsafeMode:2, // failsafe output mode (eServoOutputFailsafeMode)
                 unused:10;      // FUTURE: When someone complains "everyone" uses inverted polarity PWM or something :/
    } val;
    uint32_t raw;
} v9_rx_config_pwm_t;

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
    v9_rx_config_pwm_t pwmChannels[16];
    uint8_t     teamraceChannel:4,
                teamracePosition:3,
                teamracePitMode:1;  // FUTURE: Enable pit mode when disabling model
    uint8_t     targetSysId;
    uint8_t     sourceSysId;
} v9_rx_config_t;

// V10 changed
// powerOnCounter 3 bits -> 2
// rateInitialIdx 4 bits -> 5
// V11 changed
// rx_config_pwm_t to add stretch and changed failsafe from 988-2012 to 476-2523

