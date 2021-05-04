#pragma once

#include <inttypes.h>
#include "targets.h"
#include "crsf_protocol.h"

struct Channels
{
    // RC data in packed format for output.
    volatile crsf_channels_s PackedRCdataOut;

    // Link Statisitics Stored as Struct
    volatile crsfPayloadLinkstatistics_s LinkStatistics;  

    volatile uint16_t ChannelData[16];
};

extern Channels channels;
