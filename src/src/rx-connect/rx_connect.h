#pragma once

#include "common.h"

/**
 * The RX's search for the TX's packet rate while disconnected.
 *
 * Each pass of the scan starts on the saved rate, then repeatedly moves to the best scoring rate
 * not yet tried this pass, rescoring from an RSSI fingerprint of the sync channel before every
 * move. Once every rate has been tried a new pass starts, so no rate is ever skipped for long
 * whatever else is on the air.
 *
 * This is the public API of rx-connect: rx_main's connection state machine calls the hooks below,
 * and all the scan and capture state lives in rx_connect.cpp. RateFingerprint is internal to it.
 */

// Start the scan on the saved rate, at boot
void rxConnectBegin();

// Call from the main loop: samples RSSI and moves to the next rate when it's time
void rxConnectUpdate(unsigned long now);

// Called by SetRFLinkRate(), to set how long to dwell on the new rate
void rxConnectRateChanged(uint8_t index, uint32_t intervalUs);

// Restart the dwell on the current rate, e.g. while waiting for a SYNC after a rate change
void rxConnectRestartDwell(unsigned long now);

// A SYNC for us arrived on this rate (from the RX ISR)
void rxConnectTentative(unsigned long now);

// A packet arrived on this rate, so dwell longer before moving on (from the RX ISR)
void rxConnectPacketReceived();

// Connected: stop scanning if lock_on_first_connection is set
void rxConnectConnected();

/**
 * The connection was lost, or a tentative lock failed, and the radio is now on rate.
 * After losing a connection a new pass starts on that rate, otherwise the current pass carries on.
 */
void rxConnectLost(bool wasConnected, uint8_t rate);

// Leaving binding mode: unlock and start a new pass immediately
void rxConnectRestartScan();
