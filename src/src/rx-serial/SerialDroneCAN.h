#pragma once

#if defined(PLATFORM_ESP32)

#include "SerialMavlink.h"
#include "canard.h"
#include "dsdl/uavcan.protocol.GetNodeInfo.h"
#include "dsdl/uavcan.tunnel.Targetted.h"

#define DRONECAN_TUNNEL_FIFO_LEN    1024
#define DRONECAN_BUF_SIZE           UAVCAN_PROTOCOL_GETNODEINFO_RESPONSE_MAX_SIZE

/**
 * Stream adapter exposing the DroneCAN tunnel.Targetted byte streams to SerialMavlink.
 * Reads return bytes received from the FC, writes queue bytes to be sent to the FC.
 */
class DroneCANTunnelStream final : public Stream {
public:
    int available() override { return fcToRx.size(); }
    int read() override { return fcToRx.size() ? fcToRx.pop() : -1; }
    int peek() override { return fcToRx.size() ? fcToRx.peek() : -1; }
    // Bulk pop, Stream's default reads byte by byte with a millis() timeout check each
    using Stream::readBytes;
    size_t readBytes(char *buffer, size_t length) override;
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t *buf, size_t len) override;
    int availableForWrite() override { return rxToFc.free(); }
    void flush() override {}

    uint32_t rxToFcDropped = 0;

    FIFO<DRONECAN_TUNNEL_FIFO_LEN> fcToRx;
    FIFO<DRONECAN_TUNNEL_FIFO_LEN> rxToFc;
};

// Private base so the stream is constructed before the SerialMavlink base that is handed a reference to it
struct DroneCANTunnelHolder {
    DroneCANTunnelStream tunnelStream;
};

/**
 * MAVLink over DroneCAN: RC is sent as dronecan.sensors.rc.RCInput and MAVLink is
 * tunnelled via uavcan.tunnel.Targetted. RC_CHANNELS_OVERRIDE is never emitted.
 */
class SerialDroneCAN final : private DroneCANTunnelHolder, public SerialMavlink {
public:
    SerialDroneCAN(int8_t txPin, int8_t rxPin);
    ~SerialDroneCAN() override;

    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;
    void processSerialInput() override;
    void sendQueuedData(uint32_t maxBytesToSend) override;

    // libcanard callbacks
    bool shouldAcceptTransfer(uint64_t *outSignature, uint16_t dataTypeId, CanardTransferType transferType);
    void onTransferReceived(CanardInstance *ins, CanardRxTransfer *transfer);

private:
    void receiveFrames();
    void processTxQueue();
    void sendNodeStatus();
    void sendTunnel();
    void handleGetNodeInfo(CanardInstance *ins, CanardRxTransfer *transfer);
    void handleTunnel(CanardRxTransfer *transfer);
    uint8_t rcInputQuality(uint16_t &status);

    bool driverStarted = false;
    bool busActive = false;     // another node is acknowledging our frames, or has been heard
    CanardInstance canard {};
    uint8_t canardPool[4096] __attribute__((aligned(8)));
    uint8_t encodeBuf[DRONECAN_BUF_SIZE];
    // Decode/encode scratch, handlers run one at a time from the main loop
    union {
        uavcan_protocol_GetNodeInfoResponse nodeInfo;
        uavcan_tunnel_Targetted tunnel;
    } scratch;

    uint32_t lastNodeStatusMs = 0;
    uint32_t lastTunnelMs = 0;
    uint32_t lastTxCompleted = 0;
    uint32_t lastTxProgressMs = 0;
    uint8_t serverNodeId = 0;   // node id of the FC, learnt from its first tunnel message

    // Extended RCInput stats, requires ArduPilot >= 4.7
    uint8_t qualitySlot = 0;
    uint8_t lastTxPower = 0;
    uint32_t lastTxPowerMs = 0;

    uint8_t nodeStatusTransferId = 0;
    uint8_t rcInputTransferId = 0;
    uint8_t tunnelTransferId = 0;

    struct {
        uint32_t rxFrames, txFrames, rcSent, tunnelInDropped, txStalls, tunnelInBytes, rxTransferErrors;
        uint16_t inputBufferPeak;
    } stats {};
    void logStats();
};

#endif
