#pragma once

#include "SerialIO.h"
#include "generic_tcp_socket.h"
#include "targets.h"
#include "device.h"

#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)

class SerialTCP : public SerialIO {
public:
    explicit SerialTCP(Stream &output, Stream &input);  // Only declare the constructor, don't implement
    virtual ~SerialTCP() override;

    // SerialIO overrides
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override { return DURATION_IMMEDIATELY; };
    void sendQueuedData(uint32_t maxBytesToSend) override;
    void processBytes(uint8_t *bytes, uint16_t size) override;
    int getMaxSerialReadSize() override { return serialReadBatchSize; }
    int getMaxSerialWriteSize() override { return TCP_BUFFER_SIZE; }

    // Additional TCP-specific methods
    void initializeTCPSocket(ConnectionMode mode, uint16_t port, IPAddress remoteIP = INADDR_NONE);

private:
    static constexpr uint16_t serialReadBatchSize = 256U;

    GenericTCPSocket tcpSocket;
    bool tcpInitialized = false;
    // Buffer for temporarily storing serial data before sending to TCP
    uint8_t tempBuffer[TCP_BUFFER_SIZE];
};

#endif
