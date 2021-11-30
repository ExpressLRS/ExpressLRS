#ifdef HAS_FIVE_WAY_BUTTON
#include "FiveWayButton.h"

#define KEY_DEBOUNCE_TIME   500
#define KEY_LONG_RPESS_TIME 1000
#define KEY_CHECK_OVER_TIME 2000


uint32_t key_debounce_start = 0;

int key = INPUT_KEY_NO_PRESS;

void FiveWayButton::init()
{ 
    pinMode(GPIO_PIN_FIVE_WAY_INPUT1, INPUT|PULLUP );
    pinMode(GPIO_PIN_FIVE_WAY_INPUT2, INPUT|PULLUP );
    pinMode(GPIO_PIN_FIVE_WAY_INPUT3, INPUT|PULLUP );

    key_state = INPUT_KEY_NO_PRESS;
    keyPressed = false;
    isLongPressed = false;
}

void FiveWayButton::handle()
{  
    if(keyPressed)
    {
        int key_down = digitalRead(GPIO_PIN_FIVE_WAY_INPUT1) << 2 |  digitalRead(GPIO_PIN_FIVE_WAY_INPUT2) << 1 |  digitalRead(GPIO_PIN_FIVE_WAY_INPUT3);
        if(key_down == INPUT_KEY_NO_PRESS)
        {
            //key released
            if(isLongPressed)
            {
                clearKeyState();
            }
            else
            {
                key_state = key;
                if((millis() - key_debounce_start) > KEY_DEBOUNCE_TIME)
                {
                    keyPressed = false;        
                }
            }
        }
        else
        {
            if((millis() - key_debounce_start) > KEY_LONG_RPESS_TIME)
            {
                isLongPressed = true;
            }
        }
    }
    else
    {
        key = digitalRead(GPIO_PIN_FIVE_WAY_INPUT1) << 2 |  digitalRead(GPIO_PIN_FIVE_WAY_INPUT2) << 1 |  digitalRead(GPIO_PIN_FIVE_WAY_INPUT3);
        if(key != INPUT_KEY_NO_PRESS)
        {
            keyPressed = true;
            isLongPressed = false;
            key_debounce_start = millis();
        }
    }   
}

void FiveWayButton::getKeyState(int *keyValue, boolean *keyLongPressed)
{
    *keyValue = key_state;
    *keyLongPressed = isLongPressed;
    if(key_state != INPUT_KEY_NO_PRESS)
    {
        clearKeyState();
    }
}

void FiveWayButton::clearKeyState()
{
   keyPressed = false;
   isLongPressed = false;
   key_state = INPUT_KEY_NO_PRESS;
}
#endif