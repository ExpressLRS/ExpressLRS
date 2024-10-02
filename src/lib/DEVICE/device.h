#pragma once

#include <stdint.h>

// duration constants which can be returned from start(), event() or timeout()
#define DURATION_IGNORE -2      // when returned from event() does not update the current timeout
#define DURATION_NEVER -1       // timeout() will not be called, only event()
#define DURATION_IMMEDIATELY 0  // timeout() will be called each loop

enum deviceEvent_t {
    EVENT_ARM_FLAG_CHANGED = 1 << 0,
    EVENT_POWER_CHANGED = 1 << 1,
    EVENT_VTX_CHANGE = 1 << 2, // Triggers change on RX SPI VTX, or VTX send on TX
    EVENT_ENTER_BIND_MODE = 1 << 3,
    EVENT_EXIT_BIND_MODE = 1 << 4,
    EVENT_MODEL_SELECTED = 1 << 5,
    EVENT_CONNECTION_CHANGED = 1 << 6,

    EVENT_CONFIG_MODEL_CHANGED = 1 << 8,
    EVENT_CONFIG_VTX_CHANGED = 1 << 9,
    EVENT_CONFIG_MAIN_CHANGED = 1 << 10, // catch-all for global config item
    EVENT_CONFIG_FAN_CHANGED = 1 << 11,
    EVENT_CONFIG_MOTION_CHANGED = 1 << 12,
    EVENT_CONFIG_BUTTON_CHANGED = 1 << 13,
    EVENT_CONFIG_UID_CHANGED = 1 << 14,
    EVENT_CONFIG_POWER_COUNT_CHANGED = 1 << 15,
    EVENT_CONFIG_PWM_CHANGE = 1 << 16,
    EVENT_CONFIG_SERIAL_CHANGE = 1 << 17
};

typedef struct {
    /**
     * @brief Called at the beginning of setup() so the device can configure IO pins etc.
     */
    bool (*initialize)();

    /**
     * @brief called at the end of setup() and returns the number of milliseconds when
     * to call timeout() function.
     */
    int (*start)();

    /**
     * @brief An event was fired in the main code, perform any required action that this
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

    /**
     * @brief a bitset telling the device framework which events this device should have it's
     * event function called for.
     */
    uint32_t subscribe;
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
 * @brief Call initialize() on all the devices registered on their appropriately registered core.
 */
void devicesInit();

/**
 * @brief Call start() on all the devices registered on their appropriately registered core.
 */
void devicesStart();

/**
 * @brief This function is called in the main loop of the application and only
 * processes devices register to the loop-core. Devices registered on alternate core(s)
 * are processing in a separate FreeRTOS task running on the alternate core(s).
 *
 * @param now current time in milliseconds
 */
void devicesUpdate(unsigned long now);

/**
 * @brief Notify the device framework that an event has occurred and on the next call to
 * deviceUpdate() the event() function of the devices should be called.
 */
void devicesTriggerEvent(uint32_t events);

/**
 * @brief Stop all the devices.
 * This destroys the FreeRTOS task running on the alternate core(s).
 */
void devicesStop();
