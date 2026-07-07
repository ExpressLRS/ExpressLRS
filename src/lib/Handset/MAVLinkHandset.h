#ifndef H_MAVLINK_HANDSET
#define H_MAVLINK_HANDSET

#if defined(TARGET_TX)

#include "handset.h"
#include "FIFO.h"
#include "CRSFConnector.h"

/**
 * @brief Handset implementation that speaks MAVLink over the module-bay UART.
 */

// holds WHOLE length-prefixed frames for chunked half-duplex out
static constexpr uint32_t MAVLINK_HANDSET_FIFO_SIZE = 512;
static constexpr uint16_t MAV_MAX_FRAME_LEN = 280; // MAVLINK_MAX_PACKET_LEN

// CRSFConnector so the ELRS param engine configures the module via CRSF frames tunneled in TUNNELs
class MAVLinkHandset : public Handset, public CRSFConnector
{
public:
    ~MAVLinkHandset(); // deregister from crsfRouter
    void Begin() override;
    void End() override;
    void handleInput() override;
    void queueTelemetry(const uint8_t *data, uint8_t len) override;
    void setPacketInterval(int32_t PacketInterval) override;
    int getMinPacketInterval() const override;
    void JustSentRFpacket() override;
    bool isMavlinkHandset() const override { return true; }

    // CRSFConnector: ELRS settings/config tunneled over MAVLink
    void forwardMessage(const crsf_header_t *message) override;
    uint8_t GetMaxPacketBytes() const override { return CRSF_MAX_PACKET_LEN; }

private:
    void processByte(uint8_t b);
    void computeChunkBudget(uint32_t periodUs); // sets maxPacketBytes
    uint8_t buildHeartbeat(uint8_t *buf);       // pack synthetic HEARTBEAT into buf, return length
    uint8_t buildRadioStatus(uint8_t *buf);     // pack RADIO_STATUS into buf, return length
    uint8_t buildSync(uint8_t *buf);            // pack EdgeTX timing correction (ETXSYNC DEBUG_VECT), return len
    bool loadDueKeepalive(uint32_t now);        // build a due keepalive into outBuf (true if one was loaded)
    bool edgetxGapOpen() const;                 // true once EdgeTX follows our advertised cadence (lean-bootstrap gate)
    bool loadNextConfigFrame();                 // pop the next wrapped config TUNNEL from the FIFO into outBuf
    bool loadNextFcFrame();                     // pop next whole FC frame from FIFO into outBuf
    bool popFrame(FIFO<MAVLINK_HANDSET_FIFO_SIZE> &fifo); // shared length-prefixed dequeue into outBuf
    void frameDownlinkByte(uint8_t b);          // reassemble whole FC frames from the raw OTA telemetry stream
    void emitDownlinkFrame(const uint8_t *frame, uint16_t len); // enqueue one whole framed FC frame (length-prefixed)
    void handleOutput(uint32_t now);            // per-RC-frame gap-sized chunk drain
    uint32_t edgetxRcPeriodUs() const;          // per-baud period EdgeTX free-runs at (MAVLINK_PERIODS; sizes budget)
    void setBaud(uint32_t baud);                // switch the handset UART baud + re-derive floor/budget (autobaud)
    uint32_t hwAutobaud();                       // HW UART0 autobaud, 0 while measuring, else snapped baud

#if defined(PLATFORM_ESP32)
    void setRxPolarity(bool inverted) const;
    void duplexSetTX() const;

    bool rxInverted = false;
    uint32_t lastPolarityProbeMs = 0;
    bool     baudMeasured = false;  // HW autobaud has set currentBaud this acquisition; then only polarity is cycled
    uint32_t baudMeasuredMs = 0;    // when baudMeasured latched (re-measure if neither polarity frames in time)
#endif
    bool baudConfirmed = false;     // a working baud latched; gates re-sweep so a brief drop holds it (no 50Hz cascade)
    bool halfDuplex = false;
    bool transmitting = false;      // half-duplex: a write is in flight, line is ours
    bool frameJustReceived = false; // set by processByte, consumed by handleInput
    uint32_t lastHeartbeatMs = 0;
    uint32_t lastRadioStatusMs = 0; // last RADIO_STATUS keepalive enqueued
    uint32_t lastReceiveMs = 0;
    uint32_t lastFcHeartbeatMs = 0; // last real HEARTBEAT seen in the downlink (0 = none yet)
    uint32_t rcFrameIntervalUs = 0; // measured RC inter-arrival (us): ~advertisedPeriodUs once EdgeTX locks (lock gate)

    // Cooperative-timing sync
    uint32_t advertisedPeriodUs = 0; // RC period we ask EdgeTX to run at (us); emitted as the ETXSYNC rate
    uint32_t lastRcFrameMicros = 0;  // micros() of last decoded RC frame -> derives rcFrameIntervalUs
    uint32_t syncLastSentMs = 0;     // last ETXSYNC DEBUG_VECT emit
    uint8_t  maxPacketBytes = 0;     // per-RC-frame chunk budget
    uint32_t currentBaud = 115200;   // live handset UART baud (autodetected)

    uint8_t  outBuf[MAV_MAX_FRAME_LEN];
    uint16_t curRemaining = 0;       // bytes of the current item not yet sent (0 = at a frame boundary)
    uint16_t curOffset = 0;          // bytes of the current item already sent

    // Reassembles WHOLE MAVLink frames from un-aligned OTA chunks; PERSISTS across disconnects to keep frame alignment
    uint8_t  inFrameBuf[MAV_MAX_FRAME_LEN]; // frame being reassembled
    uint16_t inFrameIdx = 0;                // bytes collected so far (0 = scanning the stream for the next start-of-frame)
    uint16_t inFrameLen = 0;                // total expected length once the header is in (0 = not yet known)
    uint8_t  inFrameHdrLen = 0;             // header length of the in-progress frame (10 V2 / 6 V1)

    FIFO<MAVLINK_HANDSET_FIFO_SIZE> downlinkFifo; // whole FC frames, 16-bit length-prefixed, awaiting chunked send
    FIFO<MAVLINK_HANDSET_FIFO_SIZE> configFifo;   // outbound config TUNNELs, 16-bit length-prefixed
};

#endif // TARGET_TX
#endif // H_MAVLINK_HANDSET
