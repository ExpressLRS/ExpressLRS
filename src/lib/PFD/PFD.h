#pragma once
#include <Arduino.h>
#include <stdio.h>
#include "../../src/targets.h"

class PFD
{
private:
    uint32_t intEventTime = 0; 
    uint32_t extEventTime = 0;  
    int32_t result;
    bool gotExtEvent;
    bool gotIntEvent;

public:
    inline void extEvent(uint32_t time) // reference (external osc)
    {
        extEventTime = time;
        gotExtEvent = true;
    }

    inline void intEvent(uint32_t time) // internal osc event
    { 
        intEventTime = time;
        gotIntEvent = true;
    }

    inline void reset()
    {
        gotExtEvent = false;
        gotIntEvent = false;
    }

    inline void calcResult()
    {
        result = (gotExtEvent && gotIntEvent) ? (int32_t)(extEventTime - intEventTime) : 0;
    }

    inline int32_t getResult()
    {
        return result;
    }
};
