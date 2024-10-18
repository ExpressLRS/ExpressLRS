#pragma once

#include "targets.h"
#include <stdio.h>

#if defined(TARGET_TX)
#define TimerIntervalUSDefault 4000
#else
#define TimerIntervalUSDefault 20000
#endif

/**
 * @brief Hardware abstraction for the hardware timer to provide precise timing
 *
 * The timer provides callback for "tick" and "tock" alternating at half the update
 * interval each.
 *
 * The timer includes a built-in frequency offset to adjust for differences in actual
 * timing rates between 2 timers. The offset can be incremented, decremented or reset
 * to 0.
 *
 * The `phaseShift` function can be used to provide a one-time adjustment to shift the
 * phase of the timer so two timers can be aligned on their tock phase.
 */
class hwTimer
{
public:
    /**
     * @brief Initialise the timer with the specified tick/tock callback functions.
     *
     * Only the tock callback is used for transmitter targets.
     *
     * The timer initialised in the stopped state and must be started with resume().
     */
    static void init(void (*callbackTick)(), void (*callbackTock)());

    /**
     * @brief Stop the timer.
     *
     * The timer is stopped and no more callbacks are performed.
     */
    static void stop();

    /**
     * @brief Resume the timer.
     *
     * The timer is started from this instant, callbacks will be called.
     * The timer fires a tock callback immediately after it is resumed.
     */
    static void resume();

    /**
     * @brief Change the interval between callbacks.
     * The time between tick and tock is half the provided interval.
     * The timer should not be running when updateInterval() is called.
     *
     * @param time in microseconds for a full tick/tock.
     */
    static void updateInterval(uint32_t time = TimerIntervalUSDefault);
    /**
     * @brief Reset the frequency offset to zero microseconds
     */
    static ICACHE_RAM_ATTR void inline resetFreqOffset() { FreqOffset = 0; }

    /**
     * @brief Increment the frequency offset by one microsecond
     */
    static ICACHE_RAM_ATTR void inline incFreqOffset() { FreqOffset++; }


    /**
     * @brief Decrement the frequency offset by one microsecond
     */
    static ICACHE_RAM_ATTR void inline decFreqOffset() { FreqOffset--; }

    /**
     * @brief Get the frequency offset
     */
    static ICACHE_RAM_ATTR int32_t inline getFreqOffset() { return FreqOffset; }

    /**
     * @brief Provides a coarse one time adjustment to the frequency to
     * align the phase of two timers.
     *
     * The phase shift is a delay that is added to the timer after the
     * next tock callback, delaying the tick after by phaseShift microseonds.
     *
     * The maximum phase shift is 1/4 of the update interval.
     *
     * @param newPhaseShift time in microseconds to delay the timer.
     */
    static void phaseShift(int32_t newPhaseShift);

    static volatile bool running;
    static volatile bool isTick;

private:
    static void callback();

    static void (*callbackTick)();
    static void (*callbackTock)();

    static volatile uint32_t HWtimerInterval;
    static volatile int32_t PhaseShift;
    static volatile int32_t FreqOffset;
};
