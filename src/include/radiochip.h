#pragma once

#include <stdint.h>

// The radio chips the firmware can drive. A single-radio build knows its chip at compile
// time, so a test of radioChip folds away; a multi-radio build (RADIO_MULTI) links more
// than one driver and sets radioChip once at boot, before the radio is started.
// The values are also those of the hardware layout's "radio_chip" key.
enum radio_chip_e : uint8_t
{
    RADIO_CHIP_SX127X = 0,
    RADIO_CHIP_SX128X = 1,
    RADIO_CHIP_LR1121 = 2,
    RADIO_CHIP_LR2021 = 3,
};

#if defined(RADIO_MULTI)
extern radio_chip_e radioChip;
#elif defined(RADIO_SX127X)
constexpr radio_chip_e radioChip = RADIO_CHIP_SX127X;
#elif defined(RADIO_SX128X)
constexpr radio_chip_e radioChip = RADIO_CHIP_SX128X;
#elif defined(RADIO_LR1121)
constexpr radio_chip_e radioChip = RADIO_CHIP_LR1121;
#elif defined(RADIO_LR2021)
constexpr radio_chip_e radioChip = RADIO_CHIP_LR2021;
#endif

// The LR11xx/LR20xx chips: Hz frequencies, a sub-GHz and a 2.4 GHz band, dual band rates
static inline bool radioIsLR()
{
    return radioChip == RADIO_CHIP_LR1121 || radioChip == RADIO_CHIP_LR2021;
}
