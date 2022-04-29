/***
 * Slimmed-down version of esp8266_waveform for the ExpressLRS project
 * 2022-04-27 Created by CapnBry
 * - Adds the ability to change every servo no matter where it is in the
 *   cycle without blocking.
 * - Removes analogWrite() functionality. If this is used, it will break
 *   this code, as the standard core_esp8266_waveform_pwm will take over
 *   the timer.
 ***/
/*
  esp8266_waveform - General purpose waveform generation and control,
                     supporting outputs on all pins in parallel.

  Copyright (c) 2018 Earle F. Philhower, III.  All rights reserved.

  The core idea is to have a programmable waveform generator with a unique
  high and low period (defined in microseconds or CPU clock cycles).  TIMER1
  is set to 1-shot mode and is always loaded with the time until the next
  edge of any live waveforms.

  Up to one waveform generator per pin supported.

  Each waveform generator is synchronized to the ESP clock cycle counter, not
  the timer.  This allows for removing interrupt jitter and delay as the
  counter always increments once per 80MHz clock.  Changes to a waveform are
  contiguous and only take effect on the next waveform transition,
  allowing for smooth transitions.

  This replaces older tone(), analogWrite(), and the Servo classes.

  Everywhere in the code where "cycles" is used, it means ESP.getCycleCount()
  clock cycle count, or an interval measured in CPU clock cycles, but not
  TIMER1 cycles (which may be 2 CPU clock cycles @ 160MHz).

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/


#include <Arduino.h>
#include "ets_sys.h"

// Maximum delay between IRQs
#define MAXIRQUS (10000)

// Waveform generator can create tones, PWM, and servos
typedef struct {
  uint32_t nextServiceCycle;   // ESP cycle timer when a transition required
  uint32_t timeHighCycles;     // Actual running waveform period (adjusted using desiredCycles)
  uint32_t timeLowCycles;      //
  uint32_t desiredHighCycles;  // Ideal waveform period to drive the error signal
  uint32_t desiredLowCycles;   //
  uint32_t nextHighLowUs;      // Waveform ideal (us) "on deck", waiting to be changed next cycle
                               // packed into 32 bits to be atomic read/write, 65535us max
  uint32_t lastEdge;           // Cycle when this generator last changed
} Waveform;

class WVFState {
public:
  Waveform waveform[17];        // State of all possible pins
  uint32_t waveformState = 0;   // Is the pin high or low, updated in NMI so no access outside the NMI code
  uint32_t waveformEnabled = 0; // Is it actively running, updated in NMI so no access outside the NMI code

  // Enable lock-free by only allowing updates to waveformState and waveformEnabled from IRQ service routine
  uint32_t waveformToEnable = 0;  // Message to the NMI handler to start a waveform on a inactive pin
  uint32_t waveformToDisable = 0; // Message to the NMI handler to disable a pin from waveform generation

  // Optimize the NMI inner loop by keeping track of the min and max GPIO that we
  // are generating.  In the common case (1 PWM) these may be the same pin and
  // we can avoid looking at the other pins.
  uint16_t startPin = 0;
  uint16_t endPin = 0;
};
static WVFState wvfState;


// Ensure everything is read/written to RAM
#define MEMBARRIER() { __asm__ volatile("" ::: "memory"); }

// Non-speed critical bits
#pragma GCC optimize ("Os")

// Interrupt on/off control
static IRAM_ATTR void timer1Interrupt();
static bool timerRunning = false;

static __attribute__((noinline)) void initTimer() {
  if (!timerRunning) {
    timer1_disable();
    ETS_FRC_TIMER1_INTR_ATTACH(NULL, NULL);
    ETS_FRC_TIMER1_NMI_INTR_ATTACH(timer1Interrupt);
    timer1_enable(TIM_DIV1, TIM_EDGE, TIM_SINGLE);
    timerRunning = true;
    timer1_write(microsecondsToClockCycles(10));
  }
}

static IRAM_ATTR void forceTimerInterrupt() {
  if (T1L > microsecondsToClockCycles(10)) {
    T1L = microsecondsToClockCycles(10);
  }
}

// If there are no more scheduled activities, shut down Timer 1.
// Otherwise, do nothing.
static void disableIdleTimer() {
 if (timerRunning && !wvfState.waveformEnabled) {
    ETS_FRC_TIMER1_NMI_INTR_ATTACH(NULL);
    timer1_disable();
    timer1_isr_init();
    timerRunning = false;
  }
}

// Start up a waveform on a pin, or change the current one.  Will change to the new
// waveform smoothly on next low->high transition.  For immediate change, stopWaveform()
// first, then it will immediately begin.
void startWaveform8266(uint8_t pin, uint32_t timeHighUS, uint32_t timeLowUS) {
   if ((pin > 16) || isFlashInterfacePin(pin) || (timeHighUS == 0)) {
    return;
  }
  Waveform *wave = &wvfState.waveform[pin];

  uint32_t mask = 1<<pin;
  MEMBARRIER();
  if (wvfState.waveformEnabled & mask) {
    wave->nextHighLowUs = (timeHighUS << 16) | timeLowUS;
    MEMBARRIER();
    // The waveform will be updated some time in the future on the next period for the signal
  } else { //  if (!(wvfState.waveformEnabled & mask)) {
    wave->timeHighCycles = wave->desiredHighCycles = microsecondsToClockCycles(timeHighUS);
    wave->timeLowCycles = wave->desiredLowCycles = microsecondsToClockCycles(timeLowUS);
    wave->nextHighLowUs = 0;
    wave->lastEdge = 0;
    wave->nextServiceCycle = ESP.getCycleCount() + microsecondsToClockCycles(1);
    wvfState.waveformToEnable |= mask;
    MEMBARRIER();
    initTimer();
    forceTimerInterrupt();
    while (wvfState.waveformToEnable) {
      delay(0); // Wait for waveform to update
      // No mem barrier here, the call to a global function implies global state updated
    }
  }
}

// Stops a waveform on a pin
void stopWaveform8266(uint8_t pin) {
  // Can't possibly need to stop anything if there is no timer active
  if (!timerRunning) {
    return;
  }
  // If user sends in a pin >16 but <32, this will always point to a 0 bit
  // If they send >=32, then the shift will result in 0 and it will also return false
  uint32_t mask = 1<<pin;
  if (wvfState.waveformEnabled & mask) {
    wvfState.waveformToDisable = mask;
    forceTimerInterrupt();
    while (wvfState.waveformToDisable) {
      MEMBARRIER(); // If it wasn't written yet, it has to be by now
      /* no-op */ // Can't delay() since stopWaveform may be called from an IRQ
    }
  }
  disableIdleTimer();
}

// Speed critical bits
#pragma GCC optimize ("O2")

// Normally would not want two copies like this, but due to different
// optimization levels the inline attribute gets lost if we try the
// other version.
static inline IRAM_ATTR uint32_t GetCycleCountIRQ() {
  uint32_t ccount;
  __asm__ __volatile__("rsr %0,ccount":"=a"(ccount));
  return ccount;
}

// Find the earliest cycle as compared to right now
static inline IRAM_ATTR uint32_t earliest(uint32_t a, uint32_t b) {
    uint32_t now = GetCycleCountIRQ();
    int32_t da = a - now;
    int32_t db = b - now;
    return (da < db) ? a : b;
}

// The SDK and hardware take some time to actually get to our NMI code, so
// decrement the next IRQ's timer value by a bit so we can actually catch the
// real CPU cycle counter we want for the waveforms.

// The SDK also sometimes is running at a different speed the the Arduino core
// so the ESP cycle counter is actually running at a variable speed.
// adjust(x) takes care of adjusting a delta clock cycle amount accordingly.
#if F_CPU == 80000000
  #define DELTAIRQ (microsecondsToClockCycles(9)/4)
  #define adjust(x) ((x) << (turbo ? 1 : 0))
#else
  #define DELTAIRQ (microsecondsToClockCycles(9)/8)
  #define adjust(x) ((x) >> 0)
#endif

// When the time to the next edge is greater than this, RTI and set another IRQ to minimize CPU usage
#define MINIRQTIME microsecondsToClockCycles(4)

static IRAM_ATTR void timer1Interrupt() {
  // Flag if the core is at 160 MHz, for use by adjust()
  bool turbo = (*(uint32_t*)0x3FF00014) & 1 ? true : false;

  uint32_t nextEventCycle = GetCycleCountIRQ() + microsecondsToClockCycles(MAXIRQUS);
  uint32_t timeoutCycle = GetCycleCountIRQ() + microsecondsToClockCycles(14);

  if (wvfState.waveformToEnable || wvfState.waveformToDisable) {
    // Handle enable/disable requests from main app
    wvfState.waveformEnabled = (wvfState.waveformEnabled & ~wvfState.waveformToDisable) | wvfState.waveformToEnable; // Set the requested waveforms on/off
    wvfState.waveformState &= ~wvfState.waveformToEnable;  // And clear the state of any just started
    wvfState.waveformToEnable = 0;
    wvfState.waveformToDisable = 0;
    // No mem barrier.  Globals must be written to RAM on ISR exit.
    // Find the first GPIO being generated by checking GCC's find-first-set (returns 1 + the bit of the first 1 in an int32_t)
    wvfState.startPin = __builtin_ffs(wvfState.waveformEnabled) - 1;
    // Find the last bit by subtracting off GCC's count-leading-zeros (no offset in this one)
    wvfState.endPin = 32 - __builtin_clz(wvfState.waveformEnabled);
  }

  bool done = false;
  if (wvfState.waveformEnabled) {
    do {
      nextEventCycle = GetCycleCountIRQ() + microsecondsToClockCycles(MAXIRQUS);

      for (auto i = wvfState.startPin; i <= wvfState.endPin; i++) {
        uint32_t mask = 1<<i;

        // If it's not on, ignore!
        if (!(wvfState.waveformEnabled & mask)) {
          continue;
        }

        Waveform *wave = &wvfState.waveform[i];
        uint32_t now = GetCycleCountIRQ();

        // Check for toggles
        int32_t cyclesToGo = wave->nextServiceCycle - now;
        if (cyclesToGo < 0) {
          uint32_t nextEdgeCycles;
          uint32_t desired = 0;
          uint32_t *timeToUpdate;
          wvfState.waveformState ^= mask;
          if (wvfState.waveformState & mask) {
            if (i == 16) {
              GP16O = 1;
            }
            GPOS = mask;

            if (wave->nextHighLowUs != 0) {
              // Copy over next full-cycle timings
              uint32_t next = wave->nextHighLowUs;
              wave->nextHighLowUs = 0; // indicate the change has taken place
              wave->timeHighCycles = wave->desiredHighCycles = microsecondsToClockCycles(next >> 16);
              wave->timeLowCycles = wave->desiredLowCycles = microsecondsToClockCycles(next & 0xffff);
              wave->lastEdge = 0;
            }
            if (wave->lastEdge) {
              desired = wave->desiredLowCycles;
              timeToUpdate = &wave->timeLowCycles;
            }
            nextEdgeCycles = wave->timeHighCycles;
          } else {
            if (i == 16) {
              GP16O = 0;
            }
            GPOC = mask;
            desired = wave->desiredHighCycles;
            timeToUpdate = &wave->timeHighCycles;
            nextEdgeCycles = wave->timeLowCycles;
          }
          if (desired) {
            desired = adjust(desired);
            int32_t err = desired - (now - wave->lastEdge);
            if (abs(err) < desired) { // If we've lost > the entire phase, ignore this error signal
                err /= 2;
                *timeToUpdate += err;
            }
          }
          nextEdgeCycles = adjust(nextEdgeCycles);
          wave->nextServiceCycle = now + nextEdgeCycles;
          wave->lastEdge = now;
        }
        nextEventCycle = earliest(nextEventCycle, wave->nextServiceCycle);
      }

      // Exit the loop if we've hit the fixed runtime limit or the next event is known to be after that timeout would occur
      uint32_t now = GetCycleCountIRQ();
      int32_t cycleDeltaNextEvent = nextEventCycle - now;
      int32_t cyclesLeftTimeout = timeoutCycle - now;
      done = (cycleDeltaNextEvent > MINIRQTIME) || (cyclesLeftTimeout < 0);
    } while (!done);
  } // if (wvfState.waveformEnabled)

  int32_t nextEventCycles = nextEventCycle - GetCycleCountIRQ();

  if (nextEventCycles < MINIRQTIME) {
    nextEventCycles = MINIRQTIME;
  }
  nextEventCycles -= DELTAIRQ;

  // Do it here instead of global function to save time and because we know it's edge-IRQ
  T1L = nextEventCycles >> (turbo ? 1 : 0);
}

