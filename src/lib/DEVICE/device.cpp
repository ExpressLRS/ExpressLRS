#include "targets.h"
#include "common.h"
#include "logging.h"
#include "helpers.h"
#include "device.h"

///////////////////////////////////////
// Even though we aren't using anything this keeps the PIO dependency analyzer happy!

#if defined(RADIO_SX127X)
#include "SX127xDriver.h"
#elif defined(RADIO_LR1121)
#include "LR1121Driver.h"
#elif defined(RADIO_SX128X)
#include "SX1280Driver.h"
#else
#error Invalid radio configuration!
#endif

#if defined(PLATFORM_ESP32)
#include <soc/soc_caps.h>
#define MULTICORE (SOC_CPU_CORES_NUM > 1)
#endif

///////////////////////////////////////

extern bool connectionHasModelMatch;

static device_affinity_t *uiDevices;
static uint8_t deviceCount;

static bool eventFired[2] = {false, false};
static connectionState_e lastConnectionState[2] = {disconnected, disconnected};
static bool lastModelMatch[2] = {false, false};

static unsigned long deviceTimeout[16] = {0};

#if MULTICORE
static TaskHandle_t xDeviceTask = NULL;
static SemaphoreHandle_t taskSemaphore;
static SemaphoreHandle_t completeSemaphore;
static void deviceTask(void *pvArgs);
#define CURRENT_CORE  xPortGetCoreID()
#else
#define CURRENT_CORE -1
#endif

void devicesRegister(device_affinity_t *devices, uint8_t count)
{
    uiDevices = devices;
    deviceCount = count;

    #if MULTICORE
        taskSemaphore = xSemaphoreCreateBinary();
        completeSemaphore = xSemaphoreCreateBinary();
        disableCore0WDT();
        xTaskCreatePinnedToCore(deviceTask, "deviceTask", 32768, NULL, 0, &xDeviceTask, 0);
    #endif
}

void devicesInit()
{
    int32_t core = CURRENT_CORE;

    for(size_t i=0 ; i<deviceCount ; i++) {
        if (uiDevices[i].core == core || core == -1) {
            if (uiDevices[i].device->initialize) {
                (uiDevices[i].device->initialize)();
            }
        }
    }
    #if MULTICORE
    if (core == 1)
    {
        xSemaphoreGive(taskSemaphore);
        xSemaphoreTake(completeSemaphore, portMAX_DELAY);
    }
    #endif
}

void devicesStart()
{
    int32_t core = CURRENT_CORE;
    unsigned long now = millis();

    for(size_t i=0 ; i<deviceCount ; i++)
    {
        if (uiDevices[i].core == core || core == -1) {
            deviceTimeout[i] = 0xFFFFFFFF;
            if (uiDevices[i].device->start)
            {
                int delay = (uiDevices[i].device->start)();
                deviceTimeout[i] = delay == DURATION_NEVER ? 0xFFFFFFFF : now + delay;
            }
        }
    }
    #if MULTICORE
    if (core == 1)
    {
        xSemaphoreGive(taskSemaphore);
        xSemaphoreTake(completeSemaphore, portMAX_DELAY);
    }
    #endif
}

void devicesStop()
{
    #if MULTICORE
    vTaskDelete(xDeviceTask);
    #endif
}

void devicesTriggerEvent()
{
    eventFired[0] = true;
    eventFired[1] = true;
    #if MULTICORE
    // Release teh semaphore so the tasks on core 0 run now
    xSemaphoreGive(taskSemaphore);
    #endif
}

static int _devicesUpdate(unsigned long now)
{
    const int32_t core = CURRENT_CORE;
    const int32_t coreMulti = (core == -1) ? 0 : core;

    bool newModelMatch = connectionHasModelMatch && teamraceHasModelMatch;
    bool handleEvents = eventFired[coreMulti] || lastConnectionState[coreMulti] != connectionState || lastModelMatch[coreMulti] != newModelMatch;
    eventFired[coreMulti] = false;
    lastConnectionState[coreMulti] = connectionState;
    lastModelMatch[coreMulti] = newModelMatch;

    for(size_t i=0 ; i<deviceCount ; i++)
    {
        if (uiDevices[i].core == core || core == -1) {
            if (handleEvents && uiDevices[i].device->event)
            {
                int delay = (uiDevices[i].device->event)();
                if (delay != DURATION_IGNORE)
                {
                    deviceTimeout[i] = delay == DURATION_NEVER ? 0xFFFFFFFF : now + delay;
                }
            }
        }
    }

    int smallest_delay = DURATION_NEVER;
    for(size_t i=0 ; i<deviceCount ; i++)
    {
        if ((uiDevices[i].core == core || core == -1) && uiDevices[i].device->timeout)
        {
            int delay = deviceTimeout[i] == 0xFFFFFFFF ? DURATION_NEVER : (int)(deviceTimeout[i]-now);
            if (now >= deviceTimeout[i])
            {
                delay = (uiDevices[i].device->timeout)();
                deviceTimeout[i] = delay == DURATION_NEVER ? 0xFFFFFFFF : now + delay;
            }
            if (delay != DURATION_NEVER)
            {
                smallest_delay = (smallest_delay == DURATION_NEVER) ? delay : std::min(smallest_delay, delay);
            }
        }
    }
    return smallest_delay;
}

void devicesUpdate(unsigned long now)
{
    _devicesUpdate(now);
}

#if MULTICORE
static void deviceTask(void *pvArgs)
{
    xSemaphoreTake(taskSemaphore, portMAX_DELAY);
    devicesInit();
    xSemaphoreGive(completeSemaphore);
    xSemaphoreTake(taskSemaphore, portMAX_DELAY);
    devicesStart();
    xSemaphoreGive(completeSemaphore);
    for (;;)
    {
        int delay = _devicesUpdate(millis());
        // sleep the core until the desired time or it's awakened by an event
        xSemaphoreTake(taskSemaphore, delay == DURATION_NEVER ? portMAX_DELAY : pdMS_TO_TICKS(delay));
    }
}
#endif
