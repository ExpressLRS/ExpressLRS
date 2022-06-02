#if defined(TARGET_TX)

#pragma once

#include <stdint.h>
#include <config.h>
#include <POWERMGNT.h>
#include <CRSF.h>
#include <logging.h>
#include <MeanAccumulator.h>

#define DYNPOWER_UPDATE_NOUPDATE -128
#define DYNPOWER_UPDATE_MISSED   -127

// Call DynamicPower_Init in setup()
void DynamicPower_Init();
// Call DynamicPower_Update from loop()
void DynamicPower_Update();
// Call DynamicPower_TelemetryUpdate from ISR with DYNPOWER_UPDATE_MISSED or ScaledSNR value
void DynamicPower_TelemetryUpdate(int8_t snrScaled);

#endif