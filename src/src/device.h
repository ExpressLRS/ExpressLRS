#pragma once

#include <functional>

// duration constants which can be returned from start(), event() or timeout()
#define DURATION_NEVER -1       // timeout() will not be called, only event()
#define DURATION_IMMEDIATELY 0  // timeout() will be called each loop

typedef struct {
    // Called at the beginning of setup() so the device can configure IO pins etc
    void (*initialize)();
    // start() is called at the end of setup() and returns the number of ms when to call timeout()
    int (*start)();
    // An event was fired, take action and return new duration for timeout call
    int (*event)(std::function<void ()> sendSpam);
    // The duration has passed so take appropriate action and return a new duration
    int (*timeout)(std::function<void ()> sendSpam);
} device_t;
