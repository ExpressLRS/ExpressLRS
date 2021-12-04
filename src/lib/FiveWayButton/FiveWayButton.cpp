#ifdef HAS_FIVE_WAY_BUTTON
#include "FiveWayButton.h"

#define KEY_DEBOUNCE_TIME   500
#define KEY_LONG_RPESS_TIME 1000
#define KEY_CHECK_OVER_TIME 2000


uint32_t key_debounce_start = 0;

int key = INPUT_KEY_NO_PRESS;


#ifdef GPIO_PIN_JOYSTICK
#define UP    0;
#define DOWN  1;
#define LEFT  2;
#define RIGHT 3;
#define ENTER 4;
#define IDLE  5;

static int16_t joyAdcValues[] = JOY_ADC_VALUES;

bool checkValue(int direction){ 
    int value = analogRead(GPIO_PIN_JOYSTICK);
    if(value < (joyAdcValues[direction] + 50) && value > (joyAdcValues[direction] - 50)){
        return true;
    } else {
        return false;
    }
}

int checkKey()
{ 
    int fuzz = 3;
    int value = analogRead(GPIO_PIN_JOYSTICK);
        
    if(value < (joyAdcValues[0] + fuzz) && value > (joyAdcValues[0] - fuzz))
        return INPUT_KEY_UP_PRESS;
    else if(value < (joyAdcValues[1] + fuzz) && value > (joyAdcValues[1] - fuzz))
        return INPUT_KEY_DOWN_PRESS;
    else if(value < (joyAdcValues[2] + fuzz) && value > (joyAdcValues[2] - fuzz))
        return INPUT_KEY_LEFT_PRESS;
    else if(value < (joyAdcValues[3] + fuzz) && value > (joyAdcValues[3] - fuzz))
        return INPUT_KEY_RIGHT_PRESS;
    else if(value < (joyAdcValues[4] + fuzz) && value > (joyAdcValues[4] - fuzz))
        return INPUT_KEY_OK_PRESS;
    else
        return INPUT_KEY_NO_PRESS;
}
#endif

void FiveWayButton::init()
{ 
    #ifndef JOY_ADC_VALUES
    pinMode(GPIO_PIN_FIVE_WAY_INPUT1, INPUT|PULLUP );
    pinMode(GPIO_PIN_FIVE_WAY_INPUT2, INPUT|PULLUP );
    pinMode(GPIO_PIN_FIVE_WAY_INPUT3, INPUT|PULLUP );
    #endif

    key_state = INPUT_KEY_NO_PRESS;
    keyPressed = false;
    isLongPressed = false;
}

void FiveWayButton::handle()
{  
    if(keyPressed)
    {
        #ifndef JOY_ADC_VALUES
        int key_down = digitalRead(GPIO_PIN_FIVE_WAY_INPUT1) << 2 |  digitalRead(GPIO_PIN_FIVE_WAY_INPUT2) << 1 |  digitalRead(GPIO_PIN_FIVE_WAY_INPUT3);
        #else 
        // Hack, we know that INPUT_KEY_NO_PRESS = 7 so for analog we would have to have it equal idle 
        int key_down = checkKey();
        #endif


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
        #ifndef JOY_ADC_VALUES
        key = digitalRead(GPIO_PIN_FIVE_WAY_INPUT1) << 2 |  digitalRead(GPIO_PIN_FIVE_WAY_INPUT2) << 1 |  digitalRead(GPIO_PIN_FIVE_WAY_INPUT3);
        #else 
        key = checkKey();
        #endif
        
        if(key != INPUT_KEY_NO_PRESS)
        {
            keyPressed = true;
            isLongPressed = false;
            key_debounce_start = millis();
        }
    }   
}

void FiveWayButton::getKeyState(int *keyValue, bool *keyLongPressed)
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