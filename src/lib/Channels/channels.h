#pragma once

#include <inttypes.h>
#include "targets.h"
#include "crsf_protocol.h"

// current and sent switch values
#define N_SWITCHES 8

struct Channels
{
    volatile crsf_channels_s
    PackedRCdataOut;  // RC data in packed format for output.
    volatile crsfPayloadLinkstatistics_s
    LinkStatistics;  // Link Statisitics Stored as Struct

    volatile uint16_t ChannelData[16];

    uint8_t CurrentSwitches[N_SWITCHES];
    //uint8_t SentSwitches[N_SWITCHES];

    // which switch should be sent in the next rc packet
    uint8_t NextSwitchIndex;

    void updateSwitchValues();
};

extern Channels channels;
