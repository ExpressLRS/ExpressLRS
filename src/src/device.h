#pragma once

typedef struct {
    void (*initialize)();
    bool (*update)(bool eventFired, unsigned long now);
} device_t;
