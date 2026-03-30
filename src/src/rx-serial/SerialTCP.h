#pragma once

#include "device.h"
#include "FIFO.h"
#include "targets.h"

#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)

#if defined(PLATFORM_ESP8266)
#include "ESPAsyncTCP.h"
#else
#include "AsyncTCP.h"
#endif

class SerialTCP {
public:
    explicit SerialTCP(Stream &output, Stream &input);
    ~SerialTCP();

    void begin(uint16_t port);
    void end();
    void handle();
    bool isActive() const { return tcpInitialized; }

private:
    static constexpr uint16_t serialReadBatchSize = 256U;
    static constexpr uint16_t tcpBufferSize = 1024U;
    static constexpr uint32_t clientTimeoutS = 10U;
    static constexpr uint16_t maxUartBytesPerHandle = 512U;
    static constexpr uint16_t uartFlushThreshold = 64U;
    static constexpr uint32_t uartIdleFlushUs = 1500U;

    Stream *_outputPort;
    Stream *_inputPort;

    AsyncServer *tcpServer = nullptr;
    AsyncClient *tcpClient = nullptr;
    bool tcpInitialized = false;

    FIFO<tcpBufferSize> uartToTcpFifo;
    FIFO<tcpBufferSize> tcpToUartFifo;
    uint8_t tempBuffer[tcpBufferSize];
    uint32_t lastUartRxAtUs = 0;
    bool uartBatchPending = false;

    void clientConnect(AsyncClient *client);
    void clientDisconnect(AsyncClient *client);
    void handleUartInput();
    void flushTcpToUart(uint32_t maxBytesToSend);
    void flushUartToTcp();
    size_t writeClient(AsyncClient *client, const uint8_t *data, size_t len);

    static void handleNewClient(void *arg, AsyncClient *client);
    static void handleDataIn(void *arg, AsyncClient *client, void *data, size_t len);
    static void handleDisconnect(void *arg, AsyncClient *client);
    static void handleTimeOut(void *arg, AsyncClient *client, uint32_t time);
    static void handleError(void *arg, AsyncClient *client, err_t error);
    static void handleAck(void *arg, AsyncClient *client, size_t len, uint32_t time);
    static void handlePoll(void *arg, AsyncClient *client);
};

void serialTcpStart(Stream &output, Stream &input, uint16_t port);
void serialTcpStop();
bool serialTcpIsActive();

extern device_t SerialTCP_device;

#endif
