#pragma once
#if defined(TARGET_RX)

#if defined(PLATFORM_ESP8266)
#include "ESPAsyncTCP.h"
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

    private:
    AsyncServer *TCPserver = nullptr;
    AsyncClient *TCPclient = nullptr;
    AsyncWebSocket *WSserver = nullptr;
    AsyncWebSocketClient *WSclient = nullptr;
    CROSSFIRE2MSP *crsf2msp = nullptr;;
    MSP2CROSSFIRE *msp2crsf = nullptr;;

    static void handleNewClient(void *arg, AsyncClient *client);
    static void handleDataIn(void *arg, AsyncClient *client, void *data, size_t len);
    static void handleDisconnect(void *arg, AsyncClient *client);
    static void handleError(void *arg, AsyncClient *client, int8_t error);

    void initConnection();
    void clientConnect(AsyncClient * client);
    void clientDisconnect(AsyncClient *client);
    void wsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void processData(AsyncClient * client, void * data, size_t len);
};

#endif