#ifndef DEVICE_NAME
#define DEVICE_NAME "HM ES915TX\0\0\0\0\0\0"
#endif

#if defined(Regulatory_Domain_EU_868)
    #define POWER_OUTPUT_VALUES {375,850,1200,1400,1700,2000,2400,2600}
#else
    #define POWER_OUTPUT_VALUES {875,1065,1200,1355,1600,1900,2400,2600}
#endif

#include "target/Frsky_TX_R9M.h"
