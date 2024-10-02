#pragma once

#include <stdint.h>

// duration constants which can be returned from start(), event() or timeout()
#define DURATION_IGNORE -2      // when returned from event() does not update the current timeout
#define DURATION_NEVER -1       // timeout() will not be called, only event()
#define DURATION_IMMEDIATELY 0  // timeout() will be called each loop

typedef struct {
    /**
     * @brief Called at the beginning of setup() so the device can configure IO pins etc.
     *
     * Devices that return false will not have their start, event or timeout functions called.
     * i.e. the devices is effectively disabled.
     */
    bool (*initialize)();

    /**
     * @brief called at the end of setup() and returns the number of milliseconds when
     * to call timeout() function.
     */
    int (*start)();

    /**
     * @brief An event was fired in the main code, perform any requierd action that this
     * device requires and return new duration for timeout() call.
     * If DURATION_IGNORE is returned, then the current timeout value kept and timeout()
     * will be called when it expires.
     */
    int (*event)();

    /**
     * @brief The current timeout duration has expired so take appropriate action and return
     * a new duration, this function should not return DURATION_IGNORE.
     */
    int (*timeout)();
} device_t;

typedef struct {
  /**
   * @brief pointer to the device handler functions
   */
  device_t *device;
  /**
   * @brief The core on which this device is executing on a multi-core SoC
   */
  int8_t core; // 0 = alternate core or 1 = loop core
} device_affinity_t;

/**
 * @brief register a list of devices to be actioned
 *
 * @param devices list of devices to register with the device framework
 * @param count number of devices in the list
 */
void devicesRegister(device_affinity_t *devices, uint8_t count);

/**
 * @brief Call initialize() on all the devices registered on their appropriately registerd core.
 */
void devicesInit();

/**
 * @brief Call start() on all the devices registered on their appropriately registerd core.
 */
void devicesStart();

/**
 * @brief This function is called in the main loop of the application and only
 * processes devices register to the loop-core. Devices registered on alternate core(s)
 * are processing in a seperate FreeRTOS task running on the alternate core(s).
 *
 * @param now current time in millisecods
 */
void devicesUpdate(unsigned long now);

/**
 * @brief Notify the device framework that an event has occurred and on the next call to
 * deviceUpdate() the event() function of the devices should be called.
 */
void devicesTriggerEvent();

/**
 * @brief Stop all the devices.
 * This destroys the FreeRTOS task runnin on the alternate core(s).
 */
void devicesStop();
