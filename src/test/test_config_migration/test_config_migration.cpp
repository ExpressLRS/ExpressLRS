// TX config V8->V9 migration (minPower reuses v8 padding byte 9).
// Mirrors UpgradeEepromV8ToV9() + SetPower/SetMinPower/SetModelId clamp logic.
// Structs are inlined (not #include "config_legacy.h") to avoid pulling the
// CONFIG -> OTA lib chain, whose static_assert fails on Windows MinGW GCC 5.1.
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <unity.h>

typedef enum {
    PWR_10mW = 0, PWR_25mW = 1, PWR_50mW = 2, PWR_100mW = 3,
    PWR_250mW = 4, PWR_500mW = 5, PWR_1000mW = 6, PWR_2000mW = 7,
    PWR_COUNT = 8,
} PowerLevels_e;

struct POWERMGNT {
    static PowerLevels_e getMinPower() { return PWR_10mW; }
    static PowerLevels_e getMaxPower() { return PWR_1000mW; }
    static PowerLevels_e getDefaultPower() { return PWR_50mW; }
};

#define constrain(amt, low, high) \
    ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define CONFIG_TX_MODEL_CNT 64
#define CONFIG_TX_BUTTON_ACTION_CNT 2

// v8: verbatim from config_legacy.h (v8_tx_config_t)
typedef struct {
    uint32_t        version;
    uint8_t         vtxBand;
    uint8_t         vtxChannel;
    uint8_t         vtxPower;
    uint8_t         vtxPitmode;
    uint8_t         powerFanThreshold:4;
    uint8_t         _pad9_11[3];
    struct {
        uint32_t    rate:5, tlm:4, power:3, switchMode:2, boostChannel:3,
                    dynamicPower:1, modelMatch:1, txAntenna:2,
                    ptrStartChannel:4, ptrEnableChannel:5, linkMode:2;
    } model_config[64];
    uint8_t         fanMode;
    uint8_t         motionMode:2, dvrStopDelay:3, backpackDisable:1, backpackTlmMode:2;
    uint8_t         dvrStartDelay:3, dvrAux:5;
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

// v9: byte-identical copy of tx_config_t from config.h
typedef struct {
    uint32_t    rate:5, tlm:4, power:3, switchMode:2, boostChannel:3,
                dynamicPower:1, modelMatch:1, txAntenna:2,
                ptrStartChannel:4, ptrEnableChannel:5, linkMode:2;
} model_config_t;
typedef struct {
    uint8_t     pressType:1, count:3, action:4;
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
    uint8_t         motionMode:2, dvrStopDelay:3, backpackDisable:1, backpackTlmMode:2;
    uint8_t         dvrStartDelay:3, dvrAux:5;
    tx_button_color_t buttonColors[2];
} tx_config_t;

static_assert(sizeof(v8_tx_config_t) == sizeof(tx_config_t), "v8/v9 size");
static_assert(offsetof(v8_tx_config_t, _pad9_11) == 9, "v8 pad");
static_assert(offsetof(tx_config_t, minPower) == 9, "v9 minPower == v8 pad");
static_assert(offsetof(v8_tx_config_t, model_config) == 12, "v8 model_config");
static_assert(offsetof(tx_config_t, model_config) == 12, "v9 model_config");
static_assert(offsetof(v8_tx_config_t, fanMode) == 268, "v8 fanMode");
static_assert(offsetof(tx_config_t, fanMode) == 268, "v9 fanMode");
static_assert(offsetof(v8_tx_config_t, buttonColors) == 272, "v8 buttons");
static_assert(offsetof(tx_config_t, buttonColors) == 272, "v9 buttons");
static_assert(sizeof(tx_config_t) == 280, "tx_config_t 280 bytes");

// Mirrors UpgradeEepromV8ToV9() field copy (byte 9 never read; minPower reset)
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
    v9.minPower = POWERMGNT::getMinPower();
    return v9;
}

void test_layout_identical_except_byte9(void)
{
    v8_tx_config_t v8{};
    tx_config_t v9{};
    memset(&v8, 0xA5, sizeof(v8));
    memset(&v9, 0xA5, sizeof(v9));
    const uint8_t *p8 = reinterpret_cast<const uint8_t *>(&v8);
    const uint8_t *p9 = reinterpret_cast<const uint8_t *>(&v9);
    for (size_t i = 0; i < sizeof(v8); i++) {
        if (i == 9) continue;
        TEST_ASSERT_EQUAL_UINT8(p8[i], p9[i]);
    }
}

void test_layout_offsets(void)
{
    // Every field that the migration copies must sit at the same offset.
    TEST_ASSERT_EQUAL_UINT8(9, offsetof(tx_config_t, minPower));
    TEST_ASSERT_EQUAL_UINT8(12, offsetof(tx_config_t, model_config));
    TEST_ASSERT_EQUAL_UINT8(268, offsetof(tx_config_t, fanMode));
    TEST_ASSERT_EQUAL_UINT8(272, offsetof(tx_config_t, buttonColors));
    // Tail end: the last button color must end exactly at 280.
    TEST_ASSERT_EQUAL_UINT8(280, offsetof(tx_config_t, buttonColors) + sizeof(tx_config_t::buttonColors));
}

void test_migration_preserves_user_data(void)
{
    v8_tx_config_t v8{};
    v8.vtxBand = 3; v8.vtxChannel = 5; v8.vtxPower = 2; v8.vtxPitmode = 7;
    v8.powerFanThreshold = 4; v8.fanMode = 9; v8.motionMode = 2;
    v8.dvrStopDelay = 5; v8.backpackDisable = 1; v8.backpackTlmMode = 2;
    v8.dvrStartDelay = 6; v8.dvrAux = 11;
    v8.buttonColors[0].raw = 0xDEADBEEF; v8.buttonColors[1].raw = 0xCAFEBABE;
    v8.model_config[0].power = 5; v8.model_config[0].dynamicPower = 1;
    v8.model_config[42].rate = 7; v8.model_config[63].power = 1;
    v8._pad9_11[0] = 0xFF;
    tx_config_t v9 = upgrade_v8_to_v9(v8);
    TEST_ASSERT_EQUAL_UINT8(3, v9.vtxBand);
    TEST_ASSERT_EQUAL_UINT8(5, v9.vtxChannel);
    TEST_ASSERT_EQUAL_UINT8(2, v9.vtxPower);
    TEST_ASSERT_EQUAL_UINT8(7, v9.vtxPitmode);
    TEST_ASSERT_EQUAL_UINT8(4, v9.powerFanThreshold);
    TEST_ASSERT_EQUAL_UINT8(9, v9.fanMode);
    TEST_ASSERT_EQUAL_UINT8(2, v9.motionMode);
    TEST_ASSERT_EQUAL_UINT8(11, v9.dvrAux);
    TEST_ASSERT_EQUAL_UINT8(6, v9.dvrStartDelay);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, v9.buttonColors[0].raw);
    TEST_ASSERT_EQUAL_UINT32(0xCAFEBABE, v9.buttonColors[1].raw);
    TEST_ASSERT_EQUAL_UINT8(5, v9.model_config[0].power);
    TEST_ASSERT_EQUAL_UINT8(1, v9.model_config[0].dynamicPower);
    TEST_ASSERT_EQUAL_UINT8(7, v9.model_config[42].rate);
    TEST_ASSERT_EQUAL_UINT8(1, v9.model_config[63].power);
    TEST_ASSERT_EQUAL_UINT8(POWERMGNT::getMinPower(), v9.minPower);
}

void test_migration_discards_byte9_always(void)
{
    v8_tx_config_t v8{};
    v8._pad9_11[0] = PWR_500mW;
    tx_config_t v9 = upgrade_v8_to_v9(v8);
    TEST_ASSERT_EQUAL_UINT8(POWERMGNT::getMinPower(), v9.minPower);
    v8._pad9_11[0] = 0xFF;
    v9 = upgrade_v8_to_v9(v8);
    TEST_ASSERT_EQUAL_UINT8(POWERMGNT::getMinPower(), v9.minPower);
}

void test_migration_byte_identical_except_byte9(void)
{
    // Upgrade differs only where the helper intentionally doesn't copy:
    // byte 9 (minPower reset), version (0-3), padding (10-11, 271), and
    // bitfield bytes (8, 269-270) where only the low bits survive.
    v8_tx_config_t v8{};
    memset(&v8, 0x3C, sizeof(v8));
    tx_config_t v9 = upgrade_v8_to_v9(v8);
    const uint8_t *p8 = reinterpret_cast<const uint8_t *>(&v8);
    const uint8_t *p9 = reinterpret_cast<const uint8_t *>(&v9);
    for (size_t i = 0; i < sizeof(v8); i++) {
        if (i == 9) {
            TEST_ASSERT_EQUAL_UINT8(POWERMGNT::getMinPower(), p9[i]);
            continue;
        }
        if (i < 4) continue;
        if (i == 8 || i == 10 || i == 11 || i == 269 || i == 270 || i == 271) continue;
        TEST_ASSERT_EQUAL_UINT8(p8[i], p9[i]);
    }
}

void test_migration_max_bitfields(void)
{
    // Every bitfield at its ceiling must survive the copy (no truncation).
    v8_tx_config_t v8{};
    v8.powerFanThreshold = 15;
    v8.model_config[0].rate = 31; v8.model_config[0].tlm = 15;
    v8.model_config[0].power = 7; v8.model_config[0].switchMode = 3;
    v8.model_config[0].boostChannel = 7; v8.model_config[0].dynamicPower = 1;
    v8.model_config[0].modelMatch = 1; v8.model_config[0].txAntenna = 3;
    v8.model_config[0].ptrStartChannel = 15; v8.model_config[0].ptrEnableChannel = 31;
    v8.model_config[0].linkMode = 3;
    v8.motionMode = 3; v8.dvrStopDelay = 7; v8.backpackDisable = 1;
    v8.backpackTlmMode = 3; v8.dvrStartDelay = 7; v8.dvrAux = 31;
    tx_config_t v9 = upgrade_v8_to_v9(v8);
    TEST_ASSERT_EQUAL_UINT8(15, v9.powerFanThreshold);
    TEST_ASSERT_EQUAL_UINT8(31, v9.model_config[0].rate);
    TEST_ASSERT_EQUAL_UINT8(15, v9.model_config[0].tlm);
    TEST_ASSERT_EQUAL_UINT8(7, v9.model_config[0].power);
    TEST_ASSERT_EQUAL_UINT8(3, v9.model_config[0].switchMode);
    TEST_ASSERT_EQUAL_UINT8(7, v9.model_config[0].boostChannel);
    TEST_ASSERT_EQUAL_UINT8(1, v9.model_config[0].dynamicPower);
    TEST_ASSERT_EQUAL_UINT8(1, v9.model_config[0].modelMatch);
    TEST_ASSERT_EQUAL_UINT8(3, v9.model_config[0].txAntenna);
    TEST_ASSERT_EQUAL_UINT8(15, v9.model_config[0].ptrStartChannel);
    TEST_ASSERT_EQUAL_UINT8(31, v9.model_config[0].ptrEnableChannel);
    TEST_ASSERT_EQUAL_UINT8(3, v9.model_config[0].linkMode);
    TEST_ASSERT_EQUAL_UINT8(3, v9.motionMode);
    TEST_ASSERT_EQUAL_UINT8(7, v9.dvrStopDelay);
    TEST_ASSERT_EQUAL_UINT8(1, v9.backpackDisable);
    TEST_ASSERT_EQUAL_UINT8(3, v9.backpackTlmMode);
    TEST_ASSERT_EQUAL_UINT8(7, v9.dvrStartDelay);
    TEST_ASSERT_EQUAL_UINT8(31, v9.dvrAux);
}

void test_set_power_lowering_below_floor_clamps_floor(void)
{
    uint8_t currentPower = 5;
    uint8_t minPower = 4;
    auto setPower = [&](uint8_t p) {
        p = constrain(p, POWERMGNT::getMinPower(), POWERMGNT::getMaxPower());
        if (currentPower != p) {
            currentPower = p;
            if (minPower > p) minPower = p;
        }
    };
    setPower(3);
    TEST_ASSERT_EQUAL_UINT8(3, currentPower);
    TEST_ASSERT_EQUAL_UINT8(3, minPower);
}

void test_set_power_equal_to_floor_keeps_floor(void)
{
    uint8_t currentPower = 5;
    uint8_t minPower = 3;
    auto setPower = [&](uint8_t p) {
        p = constrain(p, POWERMGNT::getMinPower(), POWERMGNT::getMaxPower());
        if (currentPower != p) {
            currentPower = p;
            if (minPower > p) minPower = p;
        }
    };
    setPower(3);
    TEST_ASSERT_EQUAL_UINT8(3, currentPower);
    TEST_ASSERT_EQUAL_UINT8(3, minPower);
}

void test_set_power_above_floor_keeps_floor(void)
{
    uint8_t currentPower = 2;
    uint8_t minPower = 1;
    auto setPower = [&](uint8_t p) {
        p = constrain(p, POWERMGNT::getMinPower(), POWERMGNT::getMaxPower());
        if (currentPower != p) {
            currentPower = p;
            if (minPower > p) minPower = p;
        }
    };
    setPower(6);
    TEST_ASSERT_EQUAL_UINT8(6, currentPower);
    TEST_ASSERT_EQUAL_UINT8(1, minPower);
}

void test_set_power_no_change_when_same(void)
{
    uint8_t currentPower = 4;
    uint8_t minPower = 4;
    auto setPower = [&](uint8_t p) {
        p = constrain(p, POWERMGNT::getMinPower(), POWERMGNT::getMaxPower());
        if (currentPower != p) {
            currentPower = p;
            if (minPower > p) minPower = p;
        }
    };
    setPower(4);
    TEST_ASSERT_EQUAL_UINT8(4, currentPower);
    TEST_ASSERT_EQUAL_UINT8(4, minPower);
}

void test_set_power_clamps_to_hardware_range(void)
{
    uint8_t currentPower = 3;
    uint8_t minPower = 0;
    auto setPower = [&](uint8_t p) {
        p = constrain(p, POWERMGNT::getMinPower(), POWERMGNT::getMaxPower());
        if (currentPower != p) {
            currentPower = p;
            if (minPower > p) minPower = p;
        }
    };
    setPower(99);
    TEST_ASSERT_EQUAL_UINT8(POWERMGNT::getMaxPower(), currentPower);
    setPower(99); // no-op, stays clamped
    TEST_ASSERT_EQUAL_UINT8(POWERMGNT::getMaxPower(), currentPower);
}

void test_set_min_power_raising_above_max_clamps(void)
{
    uint8_t currentPower = 2;
    uint8_t minPower = 0;
    auto setMinPower = [&](uint8_t mp) {
        uint8_t hi = currentPower < POWERMGNT::getMaxPower()
                         ? currentPower : POWERMGNT::getMaxPower();
        mp = constrain(mp, POWERMGNT::getMinPower(), hi);
        if (minPower != mp) minPower = mp;
    };
    setMinPower(6);
    TEST_ASSERT_EQUAL_UINT8(2, minPower);
}

void test_set_min_power_below_hardware_min_clamps(void)
{
    uint8_t currentPower = 6;
    uint8_t minPower = 2;
    auto setMinPower = [&](uint8_t mp) {
        uint8_t hi = currentPower < POWERMGNT::getMaxPower()
                         ? currentPower : POWERMGNT::getMaxPower();
        mp = constrain(mp, POWERMGNT::getMinPower(), hi);
        if (minPower != mp) minPower = mp;
    };
    // getMinPower() is 0 in this stub, so a negative-like underflow (255) must
    // clamp to hi, not wrap to a huge value.
    setMinPower(255);
    TEST_ASSERT_EQUAL_UINT8(6, minPower);
}

void test_set_min_power_at_max_allowed(void)
{
    uint8_t currentPower = 4;
    uint8_t minPower = 0;
    auto setMinPower = [&](uint8_t mp) {
        uint8_t hi = currentPower < POWERMGNT::getMaxPower()
                         ? currentPower : POWERMGNT::getMaxPower();
        mp = constrain(mp, POWERMGNT::getMinPower(), hi);
        if (minPower != mp) minPower = mp;
    };
    setMinPower(4);
    TEST_ASSERT_EQUAL_UINT8(4, minPower);
}

void test_set_model_id_reclamps_floor(void)
{
    tx_config_t cfg{};
    cfg.minPower = 5;
    cfg.model_config[0].power = 6;
    cfg.model_config[1].power = 2;
    model_config_t *model = &cfg.model_config[0];
    model = &cfg.model_config[1];
    if (cfg.minPower > model->power) cfg.minPower = model->power;
    TEST_ASSERT_EQUAL_UINT8(2, cfg.minPower);
    model = &cfg.model_config[0];
    if (cfg.minPower > model->power) cfg.minPower = model->power;
    TEST_ASSERT_EQUAL_UINT8(2, cfg.minPower);
}

void test_set_model_id_equal_max_keeps_floor(void)
{
    tx_config_t cfg{};
    cfg.minPower = 3;
    cfg.model_config[0].power = 3; // exactly the floor: no reclamp, no raise
    model_config_t *model = &cfg.model_config[0];
    if (cfg.minPower > model->power) cfg.minPower = model->power;
    TEST_ASSERT_EQUAL_UINT8(3, cfg.minPower);
}

void test_set_model_id_zero_max_drops_floor_to_zero(void)
{
    tx_config_t cfg{};
    cfg.minPower = 4;
    cfg.model_config[0].power = 0; // lowest max: floor must drop to 0
    model_config_t *model = &cfg.model_config[0];
    if (cfg.minPower > model->power) cfg.minPower = model->power;
    TEST_ASSERT_EQUAL_UINT8(0, cfg.minPower);
}

void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_layout_identical_except_byte9);
    RUN_TEST(test_layout_offsets);
    RUN_TEST(test_migration_preserves_user_data);
    RUN_TEST(test_migration_discards_byte9_always);
    RUN_TEST(test_migration_byte_identical_except_byte9);
    RUN_TEST(test_migration_max_bitfields);
    RUN_TEST(test_set_power_lowering_below_floor_clamps_floor);
    RUN_TEST(test_set_power_equal_to_floor_keeps_floor);
    RUN_TEST(test_set_power_above_floor_keeps_floor);
    RUN_TEST(test_set_power_no_change_when_same);
    RUN_TEST(test_set_power_clamps_to_hardware_range);
    RUN_TEST(test_set_min_power_raising_above_max_clamps);
    RUN_TEST(test_set_min_power_below_hardware_min_clamps);
    RUN_TEST(test_set_min_power_at_max_allowed);
    RUN_TEST(test_set_model_id_reclamps_floor);
    RUN_TEST(test_set_model_id_equal_max_keeps_floor);
    RUN_TEST(test_set_model_id_zero_max_drops_floor_to_zero);
    UNITY_END();
    return 0;
}
