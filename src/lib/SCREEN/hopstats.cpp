#include "hopstats.h"
#include <string.h>

#if !defined(ICACHE_RAM_ATTR)
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
#include <Arduino.h>
#else
#define ICACHE_RAM_ATTR
#endif
#endif

hopstat_t hopStats[HOPSTATS_MAX_CHANNELS];
volatile uint8_t hopStatsCurrent;
volatile uint32_t hopStatsSlots;

// ok: 1/8 step exponential average towards 255 or 0
void ICACHE_RAM_ATTR hopStatsHit(uint8_t channel, int8_t rssi)
{
    if (channel >= HOPSTATS_MAX_CHANNELS)
        return;
    hopstat_t &s = hopStats[channel];
    s.ok = s.ok + ((255 - s.ok + 7) >> 3);
    s.rssi = (s.rssi == 0) ? rssi : (int8_t)((s.rssi * 3 + rssi) / 4);
    if (s.hits < 255)
        s.hits++;
    s.last = rssi;
    if (s.rowBest == 0 || rssi > s.rowBest)
        s.rowBest = rssi;
    hopStatsCurrent = channel;
    hopStatsSlots++;
}

void ICACHE_RAM_ATTR hopStatsMiss(uint8_t channel)
{
    if (channel >= HOPSTATS_MAX_CHANNELS)
        return;
    hopstat_t &s = hopStats[channel];
    s.ok = s.ok - (s.ok >> 3);
    s.last = 0;
    if (s.rowMiss < 255)
        s.rowMiss++;
    hopStatsCurrent = channel;
    hopStatsSlots++;
}

void hopStatsReset()
{
    memset(hopStats, 0, sizeof(hopStats));
    hopStatsSlots = 0;
}
