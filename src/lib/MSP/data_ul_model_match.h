#pragma once

#include <cstdint>

#include "msptypes.h"

/**
 * Receiver-local WiFi control remains available to a bound receiver on a
 * model mismatch. Every other completed uplink payload can reach the flight
 * controller and therefore requires a matching model.
 */
constexpr bool shouldProcessDataUlPayload(uint8_t payloadType, bool modelMatched)
{
    return modelMatched || payloadType == MSP_ELRS_SET_RX_WIFI_MODE;
}
