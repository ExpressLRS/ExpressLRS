#pragma once

typedef enum
{
    PWR_10mW = 0,
    PWR_25mW = 1,
    PWR_50mW = 2,
    PWR_100mW = 3,
    PWR_250mW = 4,
    PWR_500mW = 5,
    PWR_1000mW = 6,
    PWR_2000mW = 7,
    PWR_COUNT = 8,
    PWR_MATCH_TX = PWR_COUNT,
} PowerLevels_e;

inline unsigned powerLevelMilliwatts(PowerLevels_e power)
{
    static constexpr unsigned values[] = {10, 25, 50, 100, 250, 500, 1000, 2000};
    return power >= PWR_10mW && power < PWR_COUNT ? values[power] : 0;
}
