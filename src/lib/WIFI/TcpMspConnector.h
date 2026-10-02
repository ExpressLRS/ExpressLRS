#pragma once
#if defined(TARGET_RX)

#if defined(PLATFORM_ESP8266)
#include "ESPAsyncTCP.h"
#include "FIFO.h"
#else
#include "AsyncTCP.h"
#endif
#include <AsyncWebSocket.h>
#include "CRSFConnector.h"
#include "crsf2msp.h"
#include "msp2crsf.h"

class TcpMspConnector final : public CRSFConnector
{
public:
    TcpMspConnector();
    void begin();

    void forwardMessage(const crsf_header_t *message) override;
    AsyncWebSocket *getWSserver() const { return WSserver; }
    void flushWS();

private:
    AsyncServer *TCPserver = nullptr;
    AsyncClient *TCPclient = nullptr;
    AsyncWebSocket *WSserver = nullptr;
    AsyncWebSocketClient *WSclient = nullptr;
    CROSSFIRE2MSP *crsf2msp = nullptr;;
    MSP2CROSSFIRE *msp2crsf = nullptr;;
    // BF Configurator can overlap 250ms live-poll MSP requests; pace them to FC responses.
    uint32_t mspRequestStart = 0;
    uint16_t mspPendingLen = 0;
    bool mspRequestInFlight = false;
    uint8_t mspPendingFrame[MSP_FRAME_MAX_LEN] {};
#if defined(PLATFORM_ESP8266)
    // ESPAsyncWebServer queues WS messages with heap allocations. ESP8285 fragments fast
    // under BF Configurator's MSP bursts, so keep the backlog in a fixed FIFO instead.
    static constexpr uint16_t WS_MSP_QUEUE_BYTES = 1024;
    FIFO<WS_MSP_QUEUE_BYTES> wsQueue;
    uint8_t wsFrame[MSP_FRAME_MAX_LEN] {};
#endif

    static void handleNewClient(void *arg, AsyncClient *client);
    static void handleDataIn(void *arg, AsyncClient *client, void *data, size_t len);
    static void handleDisconnect(void *arg, AsyncClient *client);
    static void handleError(void *arg, AsyncClient *client, int8_t error);

    void initConnection();
    void clientConnect(AsyncClient * client);
    void clientDisconnect(AsyncClient *client);
    void wsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void processData(AsyncClient * client, void * data, size_t len);
    void handleInput(const uint8_t *data, size_t len);
    void processQueuedInput();
};

#endif