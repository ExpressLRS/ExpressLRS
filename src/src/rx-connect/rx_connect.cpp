#include "rx_connect.h"

#include "FHSS.h"
#include "LQCALC.h"
#include "config.h"
#include "logging.h"
#include "options.h"

// From rx_main.cpp
extern uint32_t LastSyncPacket;
extern LQCALC<100> LQCalc;
extern LQCALC<100> LQCalcDVDA;
void SetRFLinkRate(uint8_t index, bool bindMode);

// Dwell multiplier after receiving a packet on the rate, or (halved) for the saved rate at boot
static constexpr uint8_t RFmodeCycleMultiplierSlow = 10;

static uint8_t scanIndex;
static uint32_t cycleInterval; // in ms
static uint32_t RFmodeLastCycled;
static uint8_t RFmodeCycleMultiplier;
static bool LockRFmode;

void rxConnectBegin()
{
    scanIndex = config.GetRateInitialIdx();
    for (int i=0 ; i<RATE_MAX ; i++)
    {
        if (isSupportedRFRate(scanIndex))
        {
            break;
        }
        scanIndex = (scanIndex + 1) % RATE_MAX;
    }
    SetRFLinkRate(scanIndex, false);
    // Start slow on the selected rate to give it the best chance
    // to connect before beginning rate cycling
    RFmodeCycleMultiplier = RFmodeCycleMultiplierSlow / 2;
}

void rxConnectRateChanged(uint8_t index, uint32_t intervalUs)
{
    // Wait for (11/10) 110% of time it takes to cycle through all freqs in FHSS table (in ms)
    cycleInterval = ((uint32_t)11U * FHSSgetChannelCount() * get_elrs_airRateConfig(index)->FHSShopInterval * intervalUs) / (10U * 1000U);
}

void ICACHE_RAM_ATTR rxConnectRestartDwell(unsigned long now)
{
    RFmodeLastCycled = now;
}

void ICACHE_RAM_ATTR rxConnectPacketReceived()
{
    // Extend sync duration since we've received a packet at this rate
    // but do not extend it indefinitely
    RFmodeCycleMultiplier = RFmodeCycleMultiplierSlow;
}

void rxConnectConnected()
{
    LockRFmode = firmwareOptions.lock_on_first_connection;
}

void rxConnectRestartScan()
{
    scanIndex = RATE_MAX;
    RFmodeLastCycled = 0;
    LockRFmode = false;
}

/* If not connected will rotate through the RF modes looking for sync
 * and blink LED
 */
void rxConnectUpdate(unsigned long now)
{
    if (connectionState == connected || connectionState == wifiUpdate || InBindingMode)
        return;

    // Actually cycle the RF mode if not LOCK_ON_FIRST_CONNECTION
    if (LockRFmode == false && (now - RFmodeLastCycled) > (cycleInterval * RFmodeCycleMultiplier))
    {
        RFmodeLastCycled = now;
        LastSyncPacket = now;           // reset this variable
        // Display the current air rate to the user as an indicator something is happening
        SetRFLinkRate(scanIndex % RATE_MAX, false); // switch between rates
        LQCalc.reset100();
        LQCalcDVDA.reset100();
        scanIndex++;
        Radio.RXnb();
        DBGLN("%u", ExpressLRS_currAirRate_Modparams->interval);

        // Skip unsupported modes for hardware with only a single LR1121 or with a single RF path
        while (!isSupportedRFRate(scanIndex % RATE_MAX))
        {
            DBGLN("Skip %u", get_elrs_airRateConfig(scanIndex % RATE_MAX)->interval);
            scanIndex++;
        }

        // Switch to FAST_SYNC if not already in it (won't be if was just connected)
        RFmodeCycleMultiplier = 1;
    } // if time to switch RF mode
}
