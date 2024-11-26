
#ifndef MODALAI_M0184_H
#define MODALAI_M0184_H

#include "ModalAI_Common.h"

// Output Power - Default to SX1276 max output
// #define POWER_OUTPUT_FIXED 127 //MAX power for 900 RXes
// TODO: investigate why higher power doesn't work
#define POWER_OUTPUT_FIXED 114 // The highest power which didn't saturate (4dbm)
// #define POWER_OUTPUT_FIXED 113 //Low power (3dbm)

#ifndef DEVICE_NAME
    #define DEVICE_NAME "ModalAI M0184"
#endif

#endif // Header guard