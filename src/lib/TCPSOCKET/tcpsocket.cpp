#if defined(TARGET_RX)

#include "tcpsocket.h"
#include "logging.h"

TCPSOCKET *TCPSOCKET::instance = NULL;

TCPSOCKET::TCPSOCKET(const uint32_t port)
{
    instance = this;
    TCPport = port;
    TCPserver = NULL;
}

void TCPSOCKET::begin()
{
    TCPserver = new AsyncServer(TCPport);
    TCPserver->onClient(handleNewClient, TCPserver);
    TCPserver->begin();
}

void TCPSOCKET::handle()
{
    if (TCPclient == NULL)
    {
        //DBGLN("no client");
        return; // nothing to do
    }

    // check timeout
    if (millis() - clientTimeoutLastData > clientTimeoutPeriod)
    {
        TCPclient = NULL;
        DBGLN("TCP client timeout");
        return;
    }

    // check if there is data to send out the TCP port
    if ((FIFOout.size()) > 0)
    {
        const uint16_t len = FIFOout.peekSize();
        if (TCPclient->canSend() && TCPclient->space() > len)
        {
            FIFOout.popSize();
            uint8_t data[len];
            FIFOout.popBytes(data, len);
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

    if (FIFOout.available(len + 2))
    {
        FIFOout.pushSize(len);
        FIFOout.pushBytes(data, len);
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
    uint16_t len = FIFOin.popSize();
    FIFOin.popBytes(data, len);
}

uint16_t TCPSOCKET::bytesReady()
{
    return FIFOin.peekSize();
}

void TCPSOCKET::handleDataIn(void *arg, AsyncClient *client, void *data, size_t len)
{
    instance->TCPclient = client;
    instance->clientTimeoutLastData = millis();

    if (instance->FIFOin.available(len + 2)) // +2 because it takes 2 bytes to store the size of the FIFO chunk
    {
        instance->FIFOin.pushSize(len);
        instance->FIFOin.pushBytes((uint8_t *)data, len);
        DBGLN("TCP IN: queued %d bytes", len);
    }
    else
    {
        DBGLN("TCP IN: buffer full! wanted: %d, free: %d", (len + 2), instance->FIFOin.free());
    }
}

void TCPSOCKET::handleError(void *arg, AsyncClient *client, int8_t error)
{
    DBGLN("\n connection error %s from client %s \n", client->errorToString(error), client->remoteIP().toString().c_str());
}

void TCPSOCKET::handleDisconnect(void *arg, AsyncClient *client)
{
    DBGLN("\n client %s disconnected \n", client->remoteIP().toString().c_str());
    instance->TCPclient = NULL;
    instance->FIFOin.flush();
    instance->FIFOout.flush();
}

void TCPSOCKET::handleTimeOut(void *arg, AsyncClient *client, uint32_t time)
{
    DBGLN("\n client ACK timeout ip: %s \n", client->remoteIP().toString().c_str());
}

bool TCPSOCKET::hasClient()
{
    return (TCPclient == NULL) ? false : true;
}

void TCPSOCKET::handleNewClient(void *arg, AsyncClient *client)
{
    DBGLN("\n new client has been connected to server, ip: %s", client->remoteIP().toString().c_str());

    instance->TCPclient = client;
    instance->clientTimeoutLastData = millis();

    // add to list
    //instance->clients.push_back(client); // not using right now

    // register events
    client->onData(handleDataIn, NULL);
    client->onError(handleError, NULL);
    client->onDisconnect(handleDisconnect, NULL);
    client->onTimeout(handleTimeOut, NULL);
}

void TCPSOCKET::stop()
{
    DBGLN("Stopping TCP socket service on port %d", TCPport);
    
    // Disconnect any active client
    if (TCPclient != NULL)
    {
        DBGLN("Closing active TCP client connection");
        // The client will be automatically cleaned up by the handleDisconnect callback
        // when the connection is closed, which will set TCPclient to NULL and flush buffers
        TCPclient->close(true); // force close
        TCPclient = NULL;        // ensure it's cleared immediately
    }
    
    // Clear any remaining data in buffers
    FIFOin.flush();
    FIFOout.flush();
    
    // Stop and cleanup the TCP server
    if (TCPserver != NULL)
    {
        DBGLN("Stopping TCP server");
        TCPserver->end();   // Stop accepting new connections
        delete TCPserver;   // Clean up server instance
        TCPserver = NULL;   // Ensure pointer is cleared
    }
    
    DBGLN("TCP socket service stopped");
}

bool TCPSOCKET::isValid()
{
    if (TCPserver == NULL)
    {
        return false;
    }
    
    return true;
}

#endif