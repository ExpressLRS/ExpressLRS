#pragma once

#include "targets.h"

#define KEY_DEBOUNCH_TIME   300
#define KEY_LONG_RPESS_TIME 888
typedef enum
{
    NO_PRESS,
    SINGLE_PRESS,
    LONG_PRESS ,
} Key_Value_t;

class KeyButton
{
    private:

    public:

        void init();
        void handle();
        void getKeyState(Key_Value_t *key_val);
        void clearKeyState();
};
