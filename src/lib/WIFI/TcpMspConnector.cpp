#if defined(TARGET_RX)

#include "TcpMspConnector.h"
#include "logging.h"

#include "CRSFRouter.h"
#include "crsf2msp.h"
#include "msp2crsf.h"

#define TCP_PORT_BETAFLIGHT 5761 //port 5761 as used by BF configurator

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
}

void TcpMspConnector::handleNewClient(void *arg, AsyncClient *client)
{
    DBGLN("\nTCPSOCKET client (%x) connected ip: %s", client, client->remoteIP().toString().c_str());
    ((TcpMspConnector *)arg)->clientConnect(client);
}

void TcpMspConnector::handleDataIn(void *arg, AsyncClient *client, void *data, const size_t len)
{
    ((TcpMspConnector *)arg)->processData(client, data, len);
}

void TcpMspConnector::handleDisconnect(void *arg, AsyncClient *client)
{
    DBGLN("\n client %s disconnected \n", client->remoteIP().toString().c_str());
    ((TcpMspConnector *)arg)->clientDisconnect(client);
}

void TcpMspConnector::handleTimeOut(void *arg, AsyncClient *client, uint32_t time)
{
    DBGLN("\nclient ACK timeout ip: %s", client->remoteIP().toString().c_str());
}

void TcpMspConnector::handleError(void *arg, AsyncClient *client, int8_t error)
{
    DBGLN("\nclient %x connection error %s", client, client->errorToString(error));
    ((TcpMspConnector *)arg)->clientDisconnect(client);
}

void TcpMspConnector::clientConnect(AsyncClient *client)
{
    if (crsf2msp == nullptr) {
        crsf2msp = new CROSSFIRE2MSP();
        msp2crsf = new MSP2CROSSFIRE();
    }
    if (TCPclient != nullptr)
    {
        crsf2msp->reset();
        TCPclient->close();
        TCPclient = client;
    }

    // register events
    client->onData(handleDataIn, this);
    client->onError(handleError, this);
    client->onDisconnect(handleDisconnect, this);
    client->onTimeout(handleTimeOut, this);
    client->setRxTimeout(clientTimeoutS);
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

void TcpMspConnector::processData(AsyncClient *client, void *data, const size_t len)
{
    TCPclient = client;
    msp2crsf->parse(this, (uint8_t *)data, len, CRSF_ADDRESS_BLUETOOTH_WIFI, CRSF_ADDRESS_FLIGHT_CONTROLLER);
}

void TcpMspConnector::forwardMessage(const crsf_header_t *message)
{
    if (TCPclient != nullptr && (message->type == CRSF_FRAMETYPE_MSP_RESP || message->type == CRSF_FRAMETYPE_MSP_REQ))
    {
        crsf2msp->parse((uint8_t *)message, [&](const uint8_t *data, const size_t len) {
            TCPclient->write((const char *)data, len);
        });
    }
}

#endif