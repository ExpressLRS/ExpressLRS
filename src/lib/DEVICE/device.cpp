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

static device_affinity_t *uiDevices;
static uint8_t deviceCount;

static uint32_t eventFired[2] = {0, 0};
static bool lastModelMatch[2] = {false, false};

static unsigned long deviceTimeout[16] = {0};

#if MULTICORE
static TaskHandle_t xDeviceTask = nullptr;
static SemaphoreHandle_t semCore0Begin = nullptr;
static SemaphoreHandle_t semCore0Complete = nullptr;
[[noreturn]] static void deviceTask(void *pvArgs);
#define CURRENT_CORE  xPortGetCoreID()
#else
#define CURRENT_CORE -1
#endif

void devicesRegister(device_affinity_t *devices, uint8_t count)
{
    uiDevices = devices;
    deviceCount = count;

    #if MULTICORE
        // devicesRegister() comes in on CORE 1 from setup()
        // but we need two tasks, one for each core to run their respective init/start/timeout functions
        // Use two semaphores to create an interlocking pattern with CORE 0 waiting for CORE 1's calls to each function
        // (both semaphores are created "taken")
        // CORE 0 blocks for semCore0Begin
        // CORE 1 devicesInit()
        // CORE 1 give semCore0Begin -- milestone 1
        // CORE 1 wait for semCore0Complete -- given in milestone 2
        // CORE 0 wait for semCore0Begin -- given in milestone 1
        // CORE 0 devicesInit()
        // CORE 0 give semCore0Complete -- milestone 2
        // repeat pattern for devicesStart()
        // CORE 0 blocks for semCore0Begin
        // ..
        semCore0Begin = xSemaphoreCreateBinary();
        semCore0Complete = xSemaphoreCreateBinary();
        disableCore0WDT();
        xTaskCreatePinnedToCore(deviceTask, "deviceTask", 32768, NULL, 0, &xDeviceTask, 0);
    #endif
}

void devicesInit()
{
    int32_t core = CURRENT_CORE;

    for(size_t i=0 ; i<deviceCount ; i++) {
        if (uiDevices[i].core == core || core == -1) {
            if (uiDevices[i].device->initialize && !(uiDevices[i].device->initialize)()) {
                uiDevices[i].device->start = nullptr;
                uiDevices[i].device->event = nullptr;
                uiDevices[i].device->timeout = nullptr;
            }
        }
    }
    #if MULTICORE
    if (core == 1)
    {
        xSemaphoreGive(semCore0Begin);
        xSemaphoreTake(semCore0Complete, portMAX_DELAY);
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
        xSemaphoreGive(semCore0Begin);
        xSemaphoreTake(semCore0Complete, portMAX_DELAY);
    }
    #endif
}

void devicesStop()
{
    #if MULTICORE
    vTaskDelete(xDeviceTask);
    #endif
}

void devicesTriggerEvent(uint32_t events)
{
    eventFired[0] |= events;
    eventFired[1] |= events;
    #if MULTICORE
    // Break the wait in deviceTask's loop. Don't do this if all the deviceStart()s haven't completed
    // or this will trigger CORE 0's interlocking startup to progress too early
    if (semCore0Complete == nullptr)
        xSemaphoreGive(semCore0Begin);
    #endif
}

static int _devicesUpdate(unsigned long now)
{
    const int32_t core = CURRENT_CORE;
    const int32_t coreMulti = (core == -1) ? 0 : core;

    bool newModelMatch = connectionHasModelMatch && teamraceHasModelMatch;
    uint32_t events = eventFired[coreMulti];
    eventFired[coreMulti] = 0;
    bool handleEvents = events != 0 || lastModelMatch[coreMulti] != newModelMatch;
    lastModelMatch[coreMulti] = newModelMatch;

    if (handleEvents)
    {
        for(size_t i=0 ; i<deviceCount ; i++)
        {
            if ((uiDevices[i].core == core || core == -1) && (uiDevices[i].device->event && (uiDevices[i].device->subscribe & events) != 0))
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
[[noreturn]] static void deviceTask(void *pvArgs)
{
    xSemaphoreTake(semCore0Begin, portMAX_DELAY);
    devicesInit();
    xSemaphoreGive(semCore0Complete);
    xSemaphoreTake(semCore0Begin, portMAX_DELAY);
    devicesStart();
    xSemaphoreGive(semCore0Complete);
    // No longer need the complete semaphore, use it as an indicator all setup has completed
    vSemaphoreDelete(semCore0Complete);
    semCore0Complete = nullptr;
    for (;;)
    {
        int delay = _devicesUpdate(millis());
        // sleep the core until the desired time, or it's awakened by an event
        xSemaphoreTake(semCore0Begin, delay == DURATION_NEVER ? portMAX_DELAY : pdMS_TO_TICKS(delay));
    }
}
#endif
