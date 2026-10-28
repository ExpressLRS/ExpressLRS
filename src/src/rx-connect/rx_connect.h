#pragma once

#include "common.h"

/**
 * The RX's search for the TX's packet rate while disconnected.
 *
 * This is the public API of rx-connect: rx_main's connection state machine calls the hooks below,
 * and all the scan state lives in rx_connect.cpp.
 */

// Start the scan on the saved rate, at boot
void rxConnectBegin();

// Call from the main loop: moves to the next rate when it's time
void rxConnectUpdate(unsigned long now);

// Called by SetRFLinkRate(), to set how long to dwell on the new rate
void rxConnectRateChanged(uint8_t index, uint32_t intervalUs);

// Restart the dwell on the current rate, e.g. a SYNC arrived or a rate change is waiting for one
void rxConnectRestartDwell(unsigned long now);

// A packet arrived on this rate, so dwell longer before moving on (from the RX ISR)
void rxConnectPacketReceived();

// Connected: stop scanning if lock_on_first_connection is set
void rxConnectConnected();

// Leaving binding mode: unlock and start the scan from the beginning immediately
void rxConnectRestartScan();
