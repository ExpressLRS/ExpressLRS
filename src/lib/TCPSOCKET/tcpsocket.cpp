#if defined(USE_MSP_WIFI)

#include "tcpsocket.h"
#include "logging.h"

#define TCP_PORT_BETAFLIGHT 5761 //port 5761 as used by BF configurator

TCPSOCKET *TCPSOCKET::instance = NULL;

void TCPSOCKET::begin()
{
    instance = this;

    TCPserver = new AsyncServer(TCP_PORT_BETAFLIGHT);
    TCPserver->onClient(handleNewClient, TCPserver);
    TCPserver->begin();
}

void TCPSOCKET::pumpData()
{
      // check is there is any data to write out
  if (crsf2msp.FIFOout.peekSize() > 0)
  {
    const uint16_t len = crsf2msp.FIFOout.popSize();
    uint8_t data[len];
    crsf2msp.FIFOout.popBytes(data, len);
    write(data, len);
  }

  // check if there is any data to read in
  const uint16_t inReady = FIFOin->peekSize();
  if (inReady > 0)
  {
    uint8_t data[inReady];
    read(data);
    msp2crsf.parse(data, inReady);
  }
}

/***
 * @brief Add a new MSP-in-CRSF data packet read from serial to start its journey to the socket
 */
void TCPSOCKET::crsfMspIn(uint8_t *data)
{
    if (!hasClient())
    {
        return;
    }

    crsf2msp.parse(data);
}

void TCPSOCKET::handle()
{
    if (!hasClient())
    {
        return;
    }

    // check timeout
    if (millis() - clientTimeoutLastData > clientTimeoutPeriod)
    {
        DBGLN("TCP client timeout");
        clientDisconnect(TCPclient);
        return;
    }

    pumpData();

    // check if there is data to send out the TCP port
    if ((FIFOout->size()) > 0)
    {
        const uint16_t len = FIFOout->peekSize();
        if (TCPclient->canSend() && TCPclient->space() > len)
        {
            FIFOout->popSize();
            uint8_t data[len];
            FIFOout->popBytes(data, len);
            TCPclient->write((const char *)data, len);
            TCPclient->send();
            DBGLN("TCP OUT SENT: Sent!: len: %d", len);
        }
        else
        {
            DBGLN("TCP OUT SENT: Have data but TCP not ready!: len: %d", len);
        }
    }
}

bool TCPSOCKET::write(uint8_t *data, uint16_t len) // doesn't send, just ques it up.
{
    if (TCPclient == NULL)
    {
        return false; // nothing to do
    }

    if (FIFOout->available(len + 2))
    {
        FIFOout->pushSize(len);
        FIFOout->pushBytes(data, len);
        DBGLN("TCP OUT QUE: queued %d bytes", len);
        return true;
    }
    else
    {
        DBGLN("TCP OUT QUE: No space in FIFOout! len: %d", len);
        return false;
    }
}

void TCPSOCKET::read(uint8_t *data)
{
    // assume we have already checked that there is data to receive and we know how much
    // we always recieve a single chunk so no need to give len parameter
    uint16_t len = FIFOin->popSize();
    FIFOin->popBytes(data, len);
}

void TCPSOCKET::handleDataIn(void *arg, AsyncClient *client, void *data, size_t len)
{
    instance->TCPclient = client;
    instance->clientTimeoutLastData = millis();

    if (instance->FIFOin->available(len + 2)) // +2 because it takes 2 bytes to store the size of the FIFO chunk
    {
        instance->FIFOin->pushSize(len);
        instance->FIFOin->pushBytes((uint8_t *)data, len);
        DBGLN("TCP IN: queued %d bytes", len);
    }
    else
    {
        DBGLN("TCP IN: buffer full! wanted: %d, free: %d", (len + 2), instance->FIFOin->free());
    }
}

void TCPSOCKET::handleError(void *arg, AsyncClient *client, int8_t error)
{
    DBGLN("\n connection error %s from client %s \n", client->errorToString(error), client->remoteIP().toString().c_str());
}

void TCPSOCKET::handleDisconnect(void *arg, AsyncClient *client)
{
    DBGLN("\n client %s disconnected \n", client->remoteIP().toString().c_str());
    instance->clientDisconnect(client);
}

void TCPSOCKET::handleTimeOut(void *arg, AsyncClient *client, uint32_t time)
{
    DBGLN("\n client ACK timeout ip: %s \n", client->remoteIP().toString().c_str());
}

bool TCPSOCKET::hasClient()
{
    return (TCPclient != nullptr);
}

void TCPSOCKET::clientConnect(AsyncClient *client)
{
    if (!FIFOin)
    {
        FIFOin = new FIFO<BUFFER_INPUT_SIZE>();
    }
    if (!FIFOout)
    {
        FIFOout = new FIFO<BUFFER_OUTPUT_SIZE>();
    }

    TCPclient = client;
    clientTimeoutLastData = millis();
}

void TCPSOCKET::clientDisconnect(AsyncClient *client)
{
    // NOTE: FIFO buffers stick around because the code doesn't account for multiple clients
    TCPclient = nullptr;
    client->close();
    delete client;
}

void TCPSOCKET::handleNewClient(void *arg, AsyncClient *client)
{
    DBGLN("\n new client has been connected to server, ip: %s", client->remoteIP().toString().c_str());

    instance->clientConnect(client);

    // register events
    client->onData(handleDataIn, NULL);
    client->onError(handleError, NULL);
    client->onDisconnect(handleDisconnect, NULL);
    client->onTimeout(handleTimeOut, NULL);
}

#endif