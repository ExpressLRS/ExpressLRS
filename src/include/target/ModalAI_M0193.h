
#ifndef MODALAI_M0193_H
#define MODALAI_M0193_H

#include "ModalAI_Common.h"

// Output Power - Default to SX1276 max output
#define POWER_OUTPUT_FIXED 0 // -4 dbm input to FEM

#define USE_SX1276_RFO_HF
#define OPT_USE_SX1276_RFO_HF true

#ifndef DEVICE_NAME
    #define DEVICE_NAME "ModalAI M0193"
#endif

#define M0139_PA // UNUSED

#endif // Header guard