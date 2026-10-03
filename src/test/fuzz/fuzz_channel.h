// The lossy radio channel: reads the test case's opcodes and decides what happens to each packet
#pragma once

#include <cstddef>
#include <cstdint>

#include "OTA.h"

// Limit on how much the moment a packet is seen to arrive may vary. Each seed picks none or this.
constexpr int MAX_ARRIVAL_JITTER_US = 20; // per packet, either way

enum Fate { DELIVER, DROP, CORRUPT, TRUNCATE, FORGE };
extern const char *const fateNames[];

extern bool rxLoopStalled; // RX loop() stalls for the rest of this TX slot, as if behind its interrupts

void channelStart(const uint8_t *data, size_t size, size_t firstOpcode, int jitterUs, uint32_t jitterSeed);
bool channelActive();
int channelJitter();
bool channelTelemetryLost();
Fate channelNextFate(uint8_t *a, uint8_t *b);
void channelDamage(Fate fate, OTA_Packet_s *pkt, uint8_t packetSize, uint8_t a, uint8_t b);
