#include "sim_tx.h"

#include <cstdint>
#include <cstring>

#include "common.h"
#include "FHSS.h"
#include "native.h"
#include "OTA.h"

#include "fuzz_harness.h"
#include "sim_hardware.h"
#include "sim_tx_ota.h"

SimTx tx;

// Timestamp for something that has not happened yet: older than any interval the TX waits for
constexpr int64_t LONG_AGO_MS = -1000000;

// True when the TX sends full-res packets
bool txIsFullRes()
{
    return tx.packetSize == OTA8_PACKET_SIZE;
}

// Clamps the switch mode to what the packet size supports
OtaSwitchMode_e txAdjustSwitchMode(uint8_t mode)
{
    if (!txIsFullRes() && mode > smHybridOr16ch)
        return smWideOr8ch;
    return (OtaSwitchMode_e)mode;
}

// True while the TX is hearing the RX's telemetry
bool txLinked()
{
    return (int64_t)(simNow() / 1000) - tx.lastTelemetryMs < tx.ratePerf->DisconnectTimeoutMs;
}

// True if the TX's switch mode carries this channel
bool txSendsChannel(unsigned ch)
{
    if (!txIsFullRes())
        return ch <= 11 || ch == 13;
    switch (tx.mode)
    {
        case smWideOr8ch: return ch <= 7 || ch == 13;
        case sm12ch: return ch <= 11 || ch == 13;
        default: return true;
    }
}

// True if the TX listens for telemetry on this slot instead of sending
bool txIsTelemetrySlot()
{
    return tx.tlmDenom != 1 && tx.nonce % tx.tlmDenom == 0;
}

// True if the TX's current hop is the sync channel
static bool txOnSyncChannel()
{
    return FHSSsequence[tx.fhssPtr] == sync_channel;
}

// Frequency of the TX's current hop
uint32_t txFreq()
{
    return FHSSconfig->freq_start + (freq_spread * FHSSsequence[tx.fhssPtr] / FREQ_SPREAD_SCALE);
}

// The TX starts now, bound to uid, having sent no sync and heard no telemetry
void txStart(uint64_t now, const uint8_t *uid)
{
    txOtaStart(uid);
    tx.lastSyncMs = LONG_AGO_MS;
    tx.lastTelemetryMs = LONG_AGO_MS;
    tx.slotStart = now;
    tx.slotStartNs = tx.slotStart * 1000;
}

// Works out when the next slot starts on the TX's own clock
void txNextSlotTime()
{
    // The TX's clock runs fast or slow against the RX's, and that offset wanders
    const int maxOffset = MAX_CLOCK_OFFSET_PPM * 1000;
    tx.clockOffsetMilliPpm += (int64_t)tx.clockDriftMilliPpmPerS * tx.rate->interval / 1000000;
    tx.clockOffsetMilliPpm = constrain(tx.clockOffsetMilliPpm, -maxOffset, maxOffset);
    tx.slotStartNs += (int64_t)tx.rate->interval * 1000 + (int64_t)tx.rate->interval * tx.clockOffsetMilliPpm / 1000000;
    tx.slotStart = tx.slotStartNs / 1000;
}

// Advances nonce and hop for the new slot, as timerCallback() in tx_main.cpp
void txNextNonce()
{
    tx.slotNum++;
    tx.nonce++;
    if (tx.nonce % tx.rate->FHSShopInterval == 0)
        tx.fhssPtr = (tx.fhssPtr + 1) % FHSSgetSequenceCount();
}

// Sync or RC packet for this slot, as SendRCdataToRF() in tx_main.cpp
void txBuildPacket(OTA_Packet_s *pkt)
{
    memset(pkt, 0, sizeof(*pkt));
    const int64_t nowMs = tx.slotStart / 1000;
    const uint32_t syncInterval = txLinked() ? tx.ratePerf->SyncPktIntervalConnected : tx.ratePerf->SyncPktIntervalDisconnected;
    const uint8_t nonceFhss = tx.nonce % tx.rate->FHSShopInterval;

    bool sync = false;
    if ((tx.syncSlot / 2) <= nonceFhss && nowMs - tx.lastSyncMs > syncInterval && txOnSyncChannel())
    {
        sync = true;
        tx.syncSlot = (tx.syncSlot + 1) % (tx.rate->FHSShopInterval * 2);
    }

    txOtaSelect(tx.mode, tx.packetSize, tx.nonce);
    if (sync)
    {
        OTA_Sync_s *s = txIsFullRes() ? &pkt->full.sync.sync : &pkt->std.sync;
        pkt->std.type = PACKET_TYPE_SYNC;
        s->fhssIndex = tx.fhssPtr;
        s->nonce = tx.nonce;
        s->rfRateEnum = tx.rate->enum_rate;
        s->switchEncMode = tx.mode;
        s->newTlmRatio = tx.tlmRatio - TLM_RATIO_NO_TLM;
        s->UID4 = UID[4];
        s->UID5 = UID[5];
        tx.lastSyncMs = nowMs;
    }
    else
    {
        txOtaPackChannels(pkt, tx.channels);
    }
    txOtaAddCrc(pkt);
}

// The TX is switched off and on again
void txPowerCycle()
{
    tx.nonce = 0;
    tx.fhssPtr = 0;
    tx.lastTelemetryMs = LONG_AGO_MS;
    trace("     TX: power cycled\n");
}
