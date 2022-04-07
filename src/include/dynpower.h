#if defined(TARGET_TX)

#pragma once

#include <stdint.h>
#include <config.h>
#include <POWERMGNT.h>
#include <CRSF.h>
#include <logging.h>

enum DynamicPowerTelemetryUpdate_e
{
    dptuMissed = -1,       // Any telemetry packet missed
    dptuNoUpdate = 0,      // No change
    dptuNewLinkstats = 1,  // New LinkStats telemetry received
};

// Call DynamicPower_Init in setup()
void DynamicPower_Init();
// Call DynamicPower_Update from loop()
void DynamicPower_Update(uint32_t now);
// Call DynamicPower_TelemetryUpdate from ISR to update the telemetry state
void DynamicPower_TelemetryUpdate(DynamicPowerTelemetryUpdate_e dptu);

#endif