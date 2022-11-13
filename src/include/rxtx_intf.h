/***
 * This file defines the interface from device units to functions in either rx_main or tx_main
 * Use this instead of drectly declaring externs in your unit
 ***/

#include "common.h"

/***
 * TX interface
 ***/
#if defined(TARGET_TX)
#endif

/***
 * RX interface
 ***/
#if defined(TARGET_RX)
uint8_t getLq();
#endif
