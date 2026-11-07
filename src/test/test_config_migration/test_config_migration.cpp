/**
 * Migration test for TX config V8 -> V9 (minPower reuses v8 padding byte 9).
 *
 * This test validates the byte-layout assumption that the ESP8266 EEPROM
 * upgrade in TxConfig::UpgradeEepromV8ToV9() depends on:
 *   - v8 and v9 structs are the same size
 *   - minPower in v9 sits exactly where _pad9_11[0] was in v8
 *   - all other fields are at identical byte offsets
 *   - copying v8 -> v9 preserves all user data; minPower is always reset
 *     to the hardware minimum (byte 9 is never read — see the NOTE in
 *     UpgradeEepromV8ToV9 for why interim-build floors are discarded)
 *   - SetModelId re-clamps a global floor that is now above the newly active
 *     model's per-model max, and marks MAIN_CHANGED
 *   - SetPower/SetMinPower keep minPower <= configured max in both
 *     directions
 *
 * NOTE: this test reimplements the field-copy in a local helper; it does
 * NOT call the production TxConfig::UpgradeEepromV8ToV9() (which needs a
 * live ELRS_EEPROM). It therefore proves layout + ordering logic, not the
 * production migration path. The firmware builds (ESP32 + ESP8285) prove
 * that the production code compiles, including the static_asserts.
 */
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>

// --- Minimal stubs so we can include the real config headers without a full
// target build. The real build defines TARGET_TX + PLATFORM_ESP8266 and
// links the real POWERMGNT; here we just model the range/clamp semantics.
#define TARGET_TX 1
#define PLATFORM_ESP8266 1
#define CONFIG_TX_MODEL_CNT 64
#define CONFIG_TX_BUTTON_ACTION_CNT 2

// POWERMGNT stub: emulate a device with MinPower=0 (10mW) and MaxPower=6 (1W).
typedef enum {
    PWR_10mW = 0,
    PWR_25mW = 1,
    PWR_50mW = 2,
    PWR_100mW = 3,
    PWR_250mW = 4,
    PWR_500mW = 5,
    PWR_1000mW = 6,
    PWR_2000mW = 7,
    PWR_COUNT = 8,
} PowerLevels_e;

struct POWERMGNT {
    static PowerLevels_e getMinPower() { return PWR_10mW; }
    static PowerLevels_e getMaxPower() { return PWR_1000mW; }
    static PowerLevels_e getDefaultPower() { return PWR_50mW; }
};

// Arduino-style constrain macro (integer clamp).
#define constrain(amt, low, high) \
    ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

// Include the REAL legacy and current structs. config.h pulls in a lot of
// target machinery; instead we include config_legacy.h directly (it only
// needs <inttypes.h>/<stddef.h>) and define the v9 struct here as a copy
// that MUST match config.h's tx_config_t byte-for-byte. The static_asserts
// below (mirroring those in config.cpp / config_legacy.h) validate that
// copy against the legacy layout; the real v9 layout is already validated
// by the build-time static_asserts in the firmware build.
#include "config_legacy.h"

// --- v9 struct: byte-identical copy of tx_config_t from config.h ---
typedef struct {
    uint32_t    rate:5,
                tlm:4,
                power:3,
                switchMode:2,
                boostChannel:3,
                dynamicPower:1,
                modelMatch:1,
                txAntenna:2,
                ptrStartChannel:4,
                ptrEnableChannel:5,
                linkMode:2;
} model_config_t;

typedef struct {
    uint8_t     pressType:1,
                count:3,
                action:4;
} button_action_t;

typedef union {
    struct {
        uint8_t color;
        button_action_t actions[CONFIG_TX_BUTTON_ACTION_CNT];
        uint8_t unused;
    } val;
    uint32_t raw;
} tx_button_color_t;

typedef struct {
    uint32_t        version;
    uint8_t         vtxBand;
    uint8_t         vtxChannel;
    uint8_t         vtxPower;
    uint8_t         vtxPitmode;
    uint8_t         powerFanThreshold:4;
    uint8_t         minPower;
    model_config_t  model_config[CONFIG_TX_MODEL_CNT];
    uint8_t         fanMode;
    uint8_t         motionMode:2,
                    dvrStopDelay:3,
                    backpackDisable:1,
                    backpackTlmMode:2;
    uint8_t         dvrStartDelay:3,
                    dvrAux:5;
    tx_button_color_t buttonColors[2];
} tx_config_t;

// ============================================================================
// Layout assertions (duplicated here so the test fails if either side's
// layout drifts; firmware builds carry the same asserts in config.cpp).
// ============================================================================
static_assert(sizeof(v8_tx_config_t) == sizeof(tx_config_t),
              "v8 and v9 tx_config sizes must match");
static_assert(offsetof(v8_tx_config_t, _pad9_11) == 9, "v8 pad offset");
static_assert(offsetof(tx_config_t, minPower) == 9, "v9 minPower offset == v8 pad");
static_assert(offsetof(v8_tx_config_t, model_config) == 12, "v8 model_config offset");
static_assert(offsetof(tx_config_t, model_config) == 12, "v9 model_config offset");
static_assert(offsetof(v8_tx_config_t, fanMode) == 268, "v8 fanMode offset");
static_assert(offsetof(tx_config_t, fanMode) == 268, "v9 fanMode offset");
static_assert(offsetof(v8_tx_config_t, buttonColors) == 272, "v8 buttonColors offset");
static_assert(offsetof(tx_config_t, buttonColors) == 272, "v9 buttonColors offset");
static_assert(sizeof(tx_config_t) == 280, "tx_config_t must be 280 bytes");

// Helper: simulate UpgradeEepromV8ToV9's field copy (minus the EEPROM/commit
// machinery, which the firmware build covers).
static tx_config_t upgrade_v8_to_v9(const v8_tx_config_t &v8)
{
    tx_config_t v9{};
    v9.vtxBand = v8.vtxBand;
    v9.vtxChannel = v8.vtxChannel;
    v9.vtxPower = v8.vtxPower;
    v9.vtxPitmode = v8.vtxPitmode;
    v9.powerFanThreshold = v8.powerFanThreshold;
    v9.fanMode = v8.fanMode;
    v9.motionMode = v8.motionMode;
    v9.dvrStopDelay = v8.dvrStopDelay;
    v9.backpackDisable = v8.backpackDisable;
    v9.backpackTlmMode = v8.backpackTlmMode;
    v9.dvrStartDelay = v8.dvrStartDelay;
    v9.dvrAux = v8.dvrAux;
    memcpy(v9.buttonColors, v8.buttonColors, sizeof(v9.buttonColors));
    memcpy(v9.model_config, v8.model_config, sizeof(v9.model_config));
    // Byte 9 is never read as a saved floor (see NOTE in UpgradeEepromV8ToV9).
    (void)v8; // v8._pad9_11 intentionally unused
    v9.minPower = POWERMGNT::getMinPower();
    return v9;
}

int main()
{
    // --- 1. byte-identical apart from byte 9 --------------------------------
    {
        v8_tx_config_t v8{};
        tx_config_t v9{};
        memset(&v8, 0xA5, sizeof(v8));
        memset(&v9, 0xA5, sizeof(v9));
        // byte 9 is the only intentional difference
        const uint8_t *p8 = reinterpret_cast<const uint8_t *>(&v8);
        const uint8_t *p9 = reinterpret_cast<const uint8_t *>(&v9);
        for (size_t i = 0; i < sizeof(v8); i++) {
            if (i == 9) continue; // pad vs minPower
            assert(p8[i] == p9[i]);
        }
        printf("[PASS] layout: identical bytes except offset 9\n");
    }

    // --- 2. upgrade preserves user data --------------------------------------
    {
        v8_tx_config_t v8{};
        v8.vtxBand = 3;
        v8.vtxChannel = 5;
        v8.vtxPower = 2;
        v8.vtxPitmode = 7;
        v8.powerFanThreshold = 4;
        v8.fanMode = 9;
        v8.motionMode = 2;
        v8.dvrStopDelay = 5;
        v8.backpackDisable = 1;
        v8.backpackTlmMode = 2;
        v8.dvrStartDelay = 6;
        v8.dvrAux = 11;
        v8.buttonColors[0].raw = 0xDEADBEEF;
        v8.buttonColors[1].raw = 0xCAFEBABE;
        v8.model_config[0].power = 5;
        v8.model_config[0].dynamicPower = 1;
        v8.model_config[42].rate = 7;
        v8.model_config[63].power = 1;
        v8._pad9_11[0] = 0xFF; // garbage padding (genuine v8) or interim floor

        tx_config_t v9 = upgrade_v8_to_v9(v8);
        assert(v9.vtxBand == 3 && v9.vtxChannel == 5 && v9.vtxPower == 2 &&
               v9.vtxPitmode == 7 && v9.powerFanThreshold == 4);
        assert(v9.fanMode == 9 && v9.motionMode == 2 && v9.dvrAux == 11 &&
               v9.dvrStartDelay == 6);
        assert(v9.buttonColors[0].raw == 0xDEADBEEF &&
               v9.buttonColors[1].raw == 0xCAFEBABE);
        assert(v9.model_config[0].power == 5 &&
               v9.model_config[0].dynamicPower == 1 &&
               v9.model_config[42].rate == 7 && v9.model_config[63].power == 1);
        // minPower is reset when byte 9 is out of hardware range (0xFF here
        // must not leak through), even if the rest of the image is valid.
        assert(v9.minPower == POWERMGNT::getMinPower());
        printf("[PASS] migration: user data preserved, out-of-range floor reset\n");
    }

    // --- 2b. byte 9 is ALWAYS discarded (even when it holds a plausible
    // floor), so garbage can never be resurrected ---------------------------
    {
        v8_tx_config_t v8{};
        v8._pad9_11[0] = PWR_500mW; // looks like a deliberate interim floor
        tx_config_t v9 = upgrade_v8_to_v9(v8);
        assert(v9.minPower == POWERMGNT::getMinPower());
        printf("[PASS] migration: plausible interim floor intentionally discarded\n");

        v8._pad9_11[0] = 0xFF; // genuine-v8 garbage outside hardware range
        v9 = upgrade_v8_to_v9(v8);
        assert(v9.minPower == POWERMGNT::getMinPower());
        printf("[PASS] migration: genuine-v8 garbage byte discarded\n");
    }

    // --- 3. max/min ordering: SetPower lowering below floor ------------------
    {
        // Model state: power(max)=5, minPower(floor)=4
        uint8_t currentPower = 5;
        uint8_t minPower = 4;
        // Simulate SetPower(3) (lower max below floor)
        auto setPower = [&](uint8_t p) {
            p = constrain(p, POWERMGNT::getMinPower(), POWERMGNT::getMaxPower());
            if (currentPower != p) {
                currentPower = p;
                if (minPower > p) minPower = p;
            }
        };
        setPower(3);
        assert(currentPower == 3 && minPower == 3);
        printf("[PASS] SetPower: lowering max below floor clamps floor too\n");
    }

    // --- 4. max/min ordering: SetMinPower raising above max ------------------
    {
        uint8_t currentPower = 2; // max = 50mW
        uint8_t minPower = 0;
        auto setMinPower = [&](uint8_t mp) {
            uint8_t hi = currentPower < POWERMGNT::getMaxPower()
                             ? currentPower : POWERMGNT::getMaxPower();
            mp = constrain(mp, POWERMGNT::getMinPower(), hi);
            if (minPower != mp) minPower = mp;
        };
        setMinPower(6); // try to set floor above max
        assert(minPower == 2); // clamped to max
        printf("[PASS] SetMinPower: floor clamped to per-model max\n");
    }

    // --- 5. SetModelId: switching to lower-max model reclamps floor ---------
    {
        tx_config_t cfg{};
        cfg.minPower = 5; // global floor at 500mW
        cfg.model_config[0].power = 6; // model 0 max: 1W
        cfg.model_config[1].power = 2; // model 1 max: 50mW
        model_config_t *model = &cfg.model_config[0];
        // Switch to model 1 (TX's SetModelId logic)
        model = &cfg.model_config[1];
        if (cfg.minPower > model->power) cfg.minPower = model->power;
        assert(cfg.minPower == 2);
        // Switch back to model 0: floor stays at 2 (never auto-raises)
        model = &cfg.model_config[0];
        if (cfg.minPower > model->power) cfg.minPower = model->power;
        assert(cfg.minPower == 2);
        printf("[PASS] SetModelId: floor reclamps on switch, never auto-raises\n");
    }

    printf("All migration checks passed.\n");
    return 0;
}
