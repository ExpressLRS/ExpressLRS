#if defined(TARGET_RX)

#include "tcpsocket.h"
#include "logging.h"

#include "CRSFRouter.h"
#include "crsf2msp.h"
#include "msp2crsf.h"

#define TCP_PORT_BETAFLIGHT 5761 //port 5761 as used by BF configurator

TCPSOCKET::TCPSOCKET() : CRSFConnector()
{
    addDevice(CRSF_ADDRESS_BLUETOOTH_WIFI);
}

void TCPSOCKET::begin()
{
    crsfRouter.addConnector(this);

    TCPserver = new AsyncServer(TCP_PORT_BETAFLIGHT);
    TCPserver->onClient(handleNewClient, this);
    TCPserver->begin();
}

void TCPSOCKET::handleNewClient(void *arg, AsyncClient *client)
{
    DBGLN("\nTCPSOCKET client (%x) connected ip: %s", client, client->remoteIP().toString().c_str());
    ((TCPSOCKET *)arg)->clientConnect(client);
}

void TCPSOCKET::handleDataIn(void *arg, AsyncClient *client, void *data, const size_t len)
{
    ((TCPSOCKET *)arg)->processData(client, data, len);
}

void TCPSOCKET::handleDisconnect(void *arg, AsyncClient *client)
{
    DBGLN("\n client %s disconnected \n", client->remoteIP().toString().c_str());
    ((TCPSOCKET *)arg)->clientDisconnect(client);
}

void TCPSOCKET::handleTimeOut(void *arg, AsyncClient *client, uint32_t time)
{
    DBGLN("\nclient ACK timeout ip: %s", client->remoteIP().toString().c_str());
}

void TCPSOCKET::handleError(void *arg, AsyncClient *client, int8_t error)
{
    DBGLN("\nclient %x connection error %s", client, client->errorToString(error));
    ((TCPSOCKET *)arg)->clientDisconnect(client);
}

void TCPSOCKET::clientConnect(AsyncClient *client)
{
    if (TCPclient != nullptr)
    {
        TCPclient->close();
        TCPclient = client;
        crsf2msp.reset();
    }

    // register events
    client->onData(handleDataIn, this);
    client->onError(handleError, this);
    client->onDisconnect(handleDisconnect, this);
    client->onTimeout(handleTimeOut, this);
    client->setRxTimeout(clientTimeoutS);
}

void TCPSOCKET::clientDisconnect(AsyncClient *client)
{
    if (client == TCPclient)
    {
        TCPclient = nullptr;
    }
    client->close();
    delete client;
}

void TCPSOCKET::processData(AsyncClient *client, void *data, size_t len)
{
    TCPclient = client;
    msp2crsf.parse(this, (uint8_t *)data, len, CRSF_ADDRESS_BLUETOOTH_WIFI, CRSF_ADDRESS_FLIGHT_CONTROLLER);
}

void TCPSOCKET::forwardMessage(const crsf_header_t *message)
{
    if (TCPclient != nullptr)
    {
        DBGLN("Got MSP frame, forwarding to client");
        crsf2msp.parse((uint8_t *)message, [&](const uint8_t *data, const size_t len) {
            TCPclient->write((const char *)data, len);
            DBGLN("TCP OUT SENT: Sent!: len: %d", len);
        });
        TCPclient->send();
    }
}

#endif