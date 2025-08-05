#pragma once

#include <stdint.h>
#include <config.h>
#include <POWERMGNT.h>
#include <CRSF.h>
#include <logging.h>

#if defined(TARGET_TX)

#include <MeanAccumulator.h>
#include <EwmaAccumulator.h>
#include <NaiveAccumulator.h>

#define DYNPOWER_UPDATE_NOUPDATE -128
#define DYNPOWER_UPDATE_MISSED   -127

// Call DynamicPower_Init in setup()
void DynamicPower_Init();
// Call DynamicPower_Update from loop()
void DynamicPower_Update(uint32_t now);
// Call DynamicPower_TelemetryUpdate from ISR with DYNPOWER_UPDATE_MISSED or ScaledSNR value
void DynamicPower_TelemetryUpdate(int8_t snrScaled);
// Call DynamicPower_SnrThresholdUpdate from LinkStatsFromOta() when LQ >= 99
void DynamicPower_SnrThresholdUpdate(int8_t snrScaled);

#endif // TARGET_TX

#if defined(TARGET_RX)

// Call DynamicPower_UpdateRx from loop()
void DynamicPower_UpdateRx(bool initialize);

#endif // TARGET_RX