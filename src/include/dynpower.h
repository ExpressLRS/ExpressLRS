#if defined(TARGET_TX)

#pragma once

#include <stdint.h>
#include <config.h>
#include <POWERMGNT.h>
#include <CRSF.h>
#include <logging.h>
#include <MeanAccumulator.h>
#include <common.h>

typedef enum
{
    dptuNoUpdate,       // No change
    dptuNewLinkstats,   // New LinkStats telemetry received
    dptuMissed,         // Any telemetry packet missed
} DynamicPowerTelemetryUpdate_e;

// Call DynamicPower_Init in setup()
void DynamicPower_Init();
// Call DynamicPower_Update from loop()
void DynamicPower_Update();
// Call DynamicPower_TelemetryUpdate from ISR to update the telemetry state
void DynamicPower_TelemetryUpdate(DynamicPowerTelemetryUpdate_e dptu);

#endif