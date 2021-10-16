#include <stdint.h>

#include "device.h"
#include "common.h"
#include "helpers.h"

// Even though we aren't using anything this keeps the PIO dependency analyzer happy!
#include "POWERMGNT.h"

static bool eventFired = false;
static device_t **uiDevices;
static uint8_t deviceCount;
static unsigned long deviceTimeout[16] = {0};

static connectionState_e lastConnectionState = disconnected;

void devicesInit(device_t **devices, uint8_t count)
{
    uiDevices = devices;
    deviceCount = count;
    for(size_t i=0 ; i<count ; i++) {
        if (uiDevices[i]->initialize) {
            (uiDevices[i]->initialize)();
        }
    }
}

void devicesStart()
{
    unsigned long now = millis();
    for(size_t i=0 ; i<deviceCount ; i++)
    {
        deviceTimeout[i] = 0xFFFFFFFF;
        if (uiDevices[i]->start)
        {
            int delay = (uiDevices[i]->start)();
            deviceTimeout[i] = delay == DURATION_NEVER ? 0xFFFFFFFF : now + delay;
        }
    }
}

void devicesTriggerEvent()
{
    eventFired = true;
}

void devicesUpdate(unsigned long now)
{
    bool handleEvents = eventFired;
    eventFired = false;
    for(size_t i=0 ; i<deviceCount ; i++)
    {
        if ((handleEvents || lastConnectionState != connectionState) && uiDevices[i]->event)
        {
            int delay = (uiDevices[i]->event)();
            if (delay != DURATION_IGNORE)
            {
                deviceTimeout[i] = delay == DURATION_NEVER ? 0xFFFFFFFF : now + delay;
            }
        }
    }
    lastConnectionState = connectionState;
    for(size_t i=0 ; i<deviceCount ; i++)
    {
        if (now > deviceTimeout[i] && uiDevices[i]->timeout)
        {
            int delay = (uiDevices[i]->timeout)();
            deviceTimeout[i] = delay == DURATION_NEVER ? 0xFFFFFFFF : now + delay;
        }
    }
}
