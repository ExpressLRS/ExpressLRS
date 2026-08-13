#if defined(TARGET_RX)

#include "TcpMspConnector.h"
#include "logging.h"

#include "CRSFRouter.h"
#include "crsf2msp.h"
#include "msp2crsf.h"

#define TCP_PORT_BETAFLIGHT     5761    // port 5761 as used by BF configurator for tcp://xxx connections
#define WS_ENDPOINT_BETAFLIGHT  "/msp"  // URI path for BF configurator ws://xxx HTTP upgrade connections

TcpMspConnector::TcpMspConnector() : CRSFConnector()
{
    addDevice(CRSF_ADDRESS_BLUETOOTH_WIFI);
}

void TcpMspConnector::begin()
{
    crsfRouter.addConnector(this);

    TCPserver = new AsyncServer(TCP_PORT_BETAFLIGHT);
    TCPserver->onClient(handleNewClient, this);
    TCPserver->begin();

    WSserver = new AsyncWebSocket(WS_ENDPOINT_BETAFLIGHT,
        [this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
            this->wsEvent(server, client, type, arg, data, len);
        });
}

void TcpMspConnector::handleNewClient(void *arg, AsyncClient *client)
{
    DBGLN("TCP(%x) connected ip %s", client, client->remoteIP().toString().c_str());
    ((TcpMspConnector *)arg)->clientConnect(client);
}

void TcpMspConnector::handleDataIn(void *arg, AsyncClient *client, void *data, const size_t len)
{
    DBGLN("TCP(%x) read %u", client, len);
    ((TcpMspConnector *)arg)->processData(client, data, len);
}

void TcpMspConnector::handleDisconnect(void *arg, AsyncClient *client)
{
    DBGLN("TCP(%x) disconnected", client);
    ((TcpMspConnector *)arg)->clientDisconnect(client);
}

void TcpMspConnector::handleError(void *arg, AsyncClient *client, int8_t error)
{
    DBGLN("TCP(%x) connection error %s", client, client->errorToString(error));
    ((TcpMspConnector *)arg)->clientDisconnect(client);
}

void TcpMspConnector::initConnection()
{
    if (crsf2msp == nullptr)
    {
        crsf2msp = new CROSSFIRE2MSP();
        msp2crsf = new MSP2CROSSFIRE();
    }
    else
    {
        crsf2msp->reset();
    }

    // Only one connection total can be open, close all existing connections
    if (TCPclient != nullptr)
    {
        TCPclient->close();
        TCPclient = nullptr;
    }

    if (WSclient != nullptr)
    {
        WSclient->close();
        WSclient = nullptr;
    }
}

void TcpMspConnector::clientConnect(AsyncClient *client)
{
    initConnection();
    TCPclient = client;

    // register events
    client->onData(handleDataIn, this);
    client->onError(handleError, this);
    client->onDisconnect(handleDisconnect, this);
}

void TcpMspConnector::clientDisconnect(AsyncClient *client)
{
    if (client == TCPclient)
    {
        TCPclient = nullptr;
    }
    client->close();
    delete client;
}

void TcpMspConnector::wsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        DBGLN("WS(%x) connected ip %s", client, client->remoteIP().toString().c_str());
        initConnection();
        WSclient = client;
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        DBGLN("WS(%x) disconnected", client);
        if (client == WSclient)
        {
            WSclient = nullptr;
            // AsyncWebsocket handles the delete
        }
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->opcode == WS_BINARY)
        {
            DBGLN("WS(%x) read %u", client, len);
            WSclient = client;
            msp2crsf->parse(this, data, len, CRSF_ADDRESS_BLUETOOTH_WIFI, CRSF_ADDRESS_FLIGHT_CONTROLLER);
        }
    }
}

void TcpMspConnector::processData(AsyncClient *client, void *data, const size_t len)
{
    TCPclient = client;
    msp2crsf->parse(this, (uint8_t *)data, len, CRSF_ADDRESS_BLUETOOTH_WIFI, CRSF_ADDRESS_FLIGHT_CONTROLLER);
}

void TcpMspConnector::forwardMessage(const crsf_header_t *message)
{
    if (message->type != CRSF_FRAMETYPE_MSP_RESP && message->type != CRSF_FRAMETYPE_MSP_REQ)
    {
        return;
    }
    if (TCPclient == nullptr && WSclient == nullptr)
    {
        return;
    }

    DBGLN("TCP(CRSF) %u", message->frame_size);
    crsf2msp->parse((uint8_t *)message, [&](const uint8_t *data, const size_t len) {
        //DBGDUMP(data, len);
        if (TCPclient)
            TCPclient->write((const char *)data, len);
        if (WSclient)
            WSclient->binary(data, len);
    });
}

#endif