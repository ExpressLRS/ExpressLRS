#pragma once

#include <inttypes.h>
#include "targets.h"

extern volatile uint16_t ChannelData[16];

// current and sent switch values
#define N_SWITCHES 8

extern uint8_t CurrentSwitches[N_SWITCHES];
extern uint8_t SentSwitches[N_SWITCHES];

// which switch should be sent in the next rc packet
extern uint8_t NextSwitchIndex;

void ICACHE_RAM_ATTR updateSwitchValues(volatile uint16_t *channels);
