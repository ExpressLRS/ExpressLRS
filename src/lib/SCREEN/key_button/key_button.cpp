
#include "key_button.h"
#include <Arduino.h>

static long t1=0;
static uint8_t get_press ;
Key_Value_t way_press;
ICACHE_RAM_ATTR void press_callback()
{
    if(millis()-t1<200)
        return;
    t1 = millis();           
    get_press = 1;
}
void KeyButton::init()
{
    pinMode(36,INPUT);
    attachInterrupt(digitalPinToInterrupt(36),press_callback,RISING);
    get_press = 0;
    way_press = NO_PRESS;
}

void KeyButton::handle()
{
    if(get_press == 1)
    {
        if(digitalRead(36))
        {
            if(millis()-t1>KEY_LONG_RPESS_TIME)
            {
                way_press = LONG_PRESS;
                get_press = 0; 
            }
        }
        else
        {
            if(millis()-t1>KEY_DEBOUNCH_TIME)
            {
                way_press = SINGLE_PRESS;
            }
        }
    }
}
void KeyButton::getKeyState(Key_Value_t *press)
{
    *press = way_press;
    if(way_press != NO_PRESS)
    {
        Serial.println(way_press);
        clearKeyState();
        t1 = millis();
        
    }
    
}

void KeyButton::clearKeyState()
{
   way_press = NO_PRESS; 
   get_press = 0;
}