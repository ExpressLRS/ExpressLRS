#pragma once

#include <stdint.h>

/*
 * Per-FHSS-channel downlink (telemetry) statistics, for the screen's
 * "LQ by hop" and waterfall views.
 *
 * The TX knows which channel it is listening on when a telemetry slot comes
 * up, so for every telemetry slot it records either a hit (with RSSI) or a
 * miss against that channel. Updates are single-byte stores from the radio
 * ISR; the screen reads them without locking and tolerates a torn read.
 */

#define HOPSTATS_MAX_CHANNELS 128

typedef struct {
    uint8_t ok;      // exponential average of telemetry success, 0..255
    int8_t rssi;     // exponential average of hit RSSI (dBm), 0 = no data yet
    uint8_t hits;    // saturating hit counter (ages down with the average)
    int8_t last;     // RSSI of the most recent hit, 0 = most recent slot missed
    int8_t rowBest;  // best hit RSSI since the waterfall last sampled, 0 = none
    uint8_t rowMiss; // misses since the waterfall last sampled
} hopstat_t;

extern hopstat_t hopStats[HOPSTATS_MAX_CHANNELS];
extern volatile uint8_t hopStatsCurrent;      // channel of the most recent telemetry slot
extern volatile uint32_t hopStatsSlots;       // total telemetry slots recorded

void hopStatsHit(uint8_t channel, int8_t rssi);
void hopStatsMiss(uint8_t channel);
void hopStatsReset();
