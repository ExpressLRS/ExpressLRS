/***
 * This file defines the interface from device units to functions in
 * either rx_main or tx_main (or rxtx_common but exposed to other units)
 * Use this instead of drectly declaring externs in your unit
 ***/

#include "common.h"

/***
 * In both RX and TX builds
 */
void EnterBindingModeSafely();
void scheduleRebootTime(unsigned long inMs);

/***
 * TX interface
 ***/
#if defined(TARGET_TX)
void SetSyncSpam();
#endif

/***
 * RX interface
 ***/
#if defined(TARGET_RX)
#include "gpsTelemetry.h"
uint8_t getLq();
// Fills out with a snapshot of the GPS driver state, or returns false if no GPS is running
bool getGpsTelemetry(gps_telemetry_t &out);
#endif
