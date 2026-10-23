// Simulated hardware underneath the real rx_main.cpp: virtual clock, hardware timer, radio, EEPROM.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct SimRadio
{
    bool listening;
    uint32_t freq;
    uint8_t bw, sf, cr, payloadLength;
    unsigned packetsSent; // telemetry packets the RX has transmitted
};

extern SimRadio simRadio;
extern std::vector<uint8_t> simSerialOut;

void simInstall();
uint64_t simNow();
// Runs every timer and radio event that is due up to t, calling afterEvent after each
void simAdvanceTo(uint64_t t, void (*afterEvent)());
// Hands a packet to the firmware as if the radio had just received it. Returns what RXdoneCallback returned.
bool simRadioReceive(const uint8_t *data, size_t len);
