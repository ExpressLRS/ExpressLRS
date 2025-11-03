#pragma once
#if defined(TARGET_RX)

#if defined(PLATFORM_ESP8266)
#include "ESPAsyncTCP.h"
#else
#include "AsyncTCP.h"
#endif
#include "CRSFConnector.h"
#include "crsf2msp.h"
#include "msp2crsf.h"

class TCPSOCKET final : public CRSFConnector
{
public:
    TCPSOCKET();
    void begin();

    void forwardMessage(const crsf_header_t *message) override;

private:
    AsyncServer *TCPserver = nullptr;
    AsyncClient *TCPclient = nullptr;
    CROSSFIRE2MSP *crsf2msp = nullptr;;
    MSP2CROSSFIRE *msp2crsf = nullptr;;

    static void handleNewClient(void *arg, AsyncClient *client);
    static void handleDataIn(void *arg, AsyncClient *client, void *data, size_t len);
    static void handleDisconnect(void *arg, AsyncClient *client);
    static void handleTimeOut(void *arg, AsyncClient *client, uint32_t time);
    static void handleError(void *arg, AsyncClient *client, int8_t error);
    static constexpr uint32_t clientTimeoutS = 2U;

    void clientConnect(AsyncClient * client);
    void clientDisconnect(AsyncClient *client);
    void processData(AsyncClient * client, void * data, size_t len);
};

#endif