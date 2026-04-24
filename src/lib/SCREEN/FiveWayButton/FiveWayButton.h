#pragma once

#include "targets.h"

typedef enum
{
    INPUT_KEY_LEFT_PRESS = 2,
    INPUT_KEY_UP_PRESS = 3,
    INPUT_KEY_RIGHT_PRESS = 4,
    INPUT_KEY_DOWN_PRESS = 5,
    INPUT_KEY_OK_PRESS = 6,
    INPUT_KEY_NO_PRESS = 7
} Input_Key_Value_t;

// Number of values in JOY_ADC_VALUES, if defined
// These must be ADC readings for {UP, DOWN, LEFT, RIGHT, ENTER, IDLE}
constexpr size_t N_JOY_ADC_VALUES = 6;

class FiveWayButton
{
private:
    int keyInProcess;
    uint32_t keyDownStart;
    bool isLongPressed;
#if defined(JOY_ADC_VALUES)
    static uint16_t joyAdcValues[N_JOY_ADC_VALUES];
    uint16_t fuzzValues[N_JOY_ADC_VALUES];
    void calcFuzzValues();
#endif

    int readKey();

public:
    FiveWayButton();
    void init();
    void update(int *keyValue, bool *keyLongPressed);

    static constexpr uint32_t KEY_DEBOUNCE_MS = 25;
    static constexpr uint32_t KEY_LONG_PRESS_MS = 1000;
};
