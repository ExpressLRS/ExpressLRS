/***
 * This file defines the interface from device units to functions in either rx_main or tx_main
 * Use this instead of drectly declaring externs in your unit
 ***/

#include "common.h"

/***
 * TX interface
 ***/
#if defined(TARGET_TX)
void SetSyncSpam();

void tx_SetPacketRateIdx(uint8_t idx, bool forceChange);
void tx_SetSwitchMode(uint8_t idx);
void tx_SetAntennaMode(uint8_t idx);
void tx_SetTlmRatio(uint8_t idx);
#endif

/***
 * RX interface
 ***/
#if defined(TARGET_RX)
uint8_t getLq();
#endif
