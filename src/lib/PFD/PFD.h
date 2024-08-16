#pragma once
#include <Arduino.h>
#include <stdio.h>
#include "targets.h"

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

    int32_t calcResult() const
    {
        // Assumes caller has verified hasResult()
        return (int32_t)(extEventTime - intEventTime);
    }

    bool hasResult() const
    {
        return gotExtEvent && gotIntEvent;
    }

    volatile uint32_t getIntEventTime() const { return intEventTime; }
    volatile uint32_t getExtEventTime() const { return extEventTime; }
};
