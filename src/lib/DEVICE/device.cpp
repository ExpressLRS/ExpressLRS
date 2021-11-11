#include <stdint.h>

#include "device.h"
#include "common.h"
#include "logging.h"
#include "helpers.h"

// Even though we aren't using anything this keeps the PIO dependency analyzer happy!
#include "POWERMGNT.h"

static volatile bool eventFired = false;
static device_affinity_t *uiDevices;
static uint8_t deviceCount;
static unsigned long deviceTimeout[16] = {0};

static connectionState_e lastConnectionState = disconnected;

#if defined(PLATFORM_ESP32)
#define LOOP_CORE xPortGetCoreID()
#else
#define LOOP_CORE -1
#endif

void devicesRegister(device_affinity_t *devices, uint8_t count)
{
    uiDevices = devices;
    deviceCount = count;
}

void devicesInit()
{
    uint32_t core = LOOP_CORE;
    for(size_t i=0 ; i<deviceCount ; i++) {
        if (uiDevices[i].core == core || core == -1) {
            if (uiDevices[i].device->initialize) {
                (uiDevices[i].device->initialize)();
            }
        }
    }
}

void devicesStart()
{
    uint32_t core = LOOP_CORE;
    unsigned long now = millis();
    for(size_t i=0 ; i<deviceCount ; i++)
    {
        deviceTimeout[i] = 0xFFFFFFFF;
        if (uiDevices[i].core == core || core == -1) {
            if (uiDevices[i].device->start)
            {
                int delay = (uiDevices[i].device->start)();
                deviceTimeout[i] = delay == DURATION_NEVER ? 0xFFFFFFFF : now + delay;
            }
        }
    }
}

void devicesTriggerEvent()
{
    eventFired = true;
}

void devicesUpdate(unsigned long now)
{
    uint32_t core = LOOP_CORE;
    bool handleEvents = eventFired;
    eventFired = false;
    for(size_t i=0 ; i<deviceCount ; i++)
    {
        if (uiDevices[i].core == core || core == -1) {
            if ((handleEvents || lastConnectionState != connectionState) && uiDevices[i].device->event)
            {
                int delay = (uiDevices[i].device->event)();
                if (delay != DURATION_IGNORE)
                {
                    deviceTimeout[i] = delay == DURATION_NEVER ? 0xFFFFFFFF : now + delay;
                }
            }
        }
    }
    lastConnectionState = connectionState;
    for(size_t i=0 ; i<deviceCount ; i++)
    {
        if (uiDevices[i].core == core || core == -1) {
            if (now > deviceTimeout[i] && uiDevices[i].device->timeout)
            {
                int delay = (uiDevices[i].device->timeout)();
                deviceTimeout[i] = delay == DURATION_NEVER ? 0xFFFFFFFF : now + delay;
            }
        }
    }
}
