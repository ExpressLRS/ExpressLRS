// Simulated transmitter. It packs with the real OTA code; its slot, sync and hop scheduling is a
// simplified copy of tx_main.cpp.
#pragma once

#include <cstdint>

#include "common.h"
#include "OTA.h"

// Limits on how far the TX's clock may be off against the RX's. Each seed picks values within them.
// The defaults are meant to be mild: two ordinary crystals, one of them warming up inside a fuselage.
constexpr int MAX_CLOCK_OFFSET_PPM = 40;        // constant offset between the two clocks
constexpr int MAX_CLOCK_DRIFT_PPM_PER_S = 1;    // how fast that offset may change

// The Makefile builds the TX packer into OTA.cpp, but OTA.h only declares it for a TX target
typedef void (*PackChannelData_t)(OTA_Packet_s *const otaPktPtr, const uint32_t *channelData, bool stubbornAck);
extern PackChannelData_t OtaPackChannelData;

struct SimTx
{
    // Fixed per test case
    expresslrs_mod_settings_s *rate;
    expresslrs_rf_pref_params_s *ratePerf;
    uint8_t packetSize;
    expresslrs_tlm_ratio_e tlmRatio;
    uint8_t tlmDenom;
    int clockDriftMilliPpmPerS;
    uint32_t channels[CRSF_NUM_CHANNELS];

    uint8_t nonce;
    uint8_t fhssPtr;
    OtaSwitchMode_e mode;
    unsigned syncSlot;
    int64_t lastSyncMs;
    int64_t lastTelemetryMs;
    unsigned slotNum;
    uint64_t slotStart;
    int64_t slotStartNs;
    int clockOffsetMilliPpm;
};

extern SimTx tx;

bool txIsFullRes();
OtaSwitchMode_e txAdjustSwitchMode(uint8_t mode);
bool txLinked();
bool txSendsChannel(unsigned ch);
bool txIsTelemetrySlot();
uint32_t txFreq();

void txStart(uint64_t now);
void txNextSlotTime();
void txNextNonce();
void txBuildPacket(OTA_Packet_s *pkt);

void txPowerCycle();
