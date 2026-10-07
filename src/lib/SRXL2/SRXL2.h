#pragma once
#include <cstdint>

namespace SRXL2
{
struct Packet { uint8_t bytes[80]; uint8_t length; };
struct Reading { int32_t value; uint32_t updatedUs; bool valid; };
struct Telemetry
{
    Reading rpm, voltage, current, temperatureFet, temperatureBec;
    Reading voltageBec, currentBec, batteryCurrent, consumption, batteryTemperature;
    Reading cells[18];
    uint8_t cellCount;
};
uint16_t crc16(uint8_t *bytes, uint8_t length);
uint16_t encodeThrottle(uint16_t value);
class Link
{
public:
    void reset(uint32_t uid, uint32_t nowUs, bool startupAckOnly = false);
    void setControlPermission(bool permitted);
    void setThrottle(uint16_t crsfValue, uint32_t nowUs);
    void missedFrame();
    void receive(uint8_t byte, uint32_t nowUs);
    bool nextPacket(uint32_t nowUs, Packet &packet);
    void transmitted(uint32_t nowUs);
    bool connected() const;
    // Continue discovery when the startup transport already acknowledged a validated ESC.
    void finishStartupDiscovery() { phase = Broadcast; }
    Telemetry telemetry(uint32_t nowUs) const;
private:
    enum Phase { Discover, Acknowledge, Broadcast, Active };
    Phase phase = Discover;
    uint32_t uid = 0, started = 0, lastHello = 0, lastBus = 0, lastByte = 0;
    uint32_t lastControl = 0, lastRequest = 0, lastReply = 0, lastSample = 0, txEnded = 0;
    uint16_t throttle = 0x8000;
    bool permitted = false, released = false, hasSample = false;
    bool txBusy = false, requestReply = false, waitingReply = false, helloPending = false;
    bool startupAckOnly = false;
    bool monitorReplies = false;
    bool urgentControl = false;
    bool broadcastTx = false, cellCountKnown = false, controlPending = false, fadePending = false;
    uint8_t input[80] = {}, inputSize = 0;
    mutable Telemetry data{}; // expiry is cached; a counter wrap must not revive it
    void restart(uint32_t nowUs);
    void processFrame(uint32_t nowUs);
    void decodeTelemetry(const uint8_t *payload, uint32_t nowUs);
    void resync();
};
}
