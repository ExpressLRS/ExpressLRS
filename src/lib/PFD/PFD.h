#pragma once
#include <Arduino.h>
#include <stdio.h>
#include "../../src/include/targets.h"

class PFD
{
private:
    uint32_t intEventTime = 0;
    uint32_t extEventTime = 0;
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

    inline bool resultValid()
    {
        return gotExtEvent && gotIntEvent;
    }
    
    // Check resultValid() before using getResult() return value
    inline int32_t getResult()
    {
        return (int32_t)(extEventTime - intEventTime);
    }

    volatile uint32_t getIntEventTime() const { return intEventTime; }
    volatile uint32_t getExtEventTime() const { return extEventTime; }
};
