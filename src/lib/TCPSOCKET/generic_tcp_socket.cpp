#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)

#include "generic_tcp_socket.h"
#include "logging.h"

GenericTCPSocket *GenericTCPSocket::instance = NULL;

AsyncClient *GenericTCPSocket::getActiveClient()
{
    if (mode == TCP_SERVER)
    {
        return currentClient;
    }

    return tcpClient;
}

void GenericTCPSocket::flushOutgoingData()
{
    AsyncClient *client = getActiveClient();
    if (client == nullptr || !client->connected() || toWifiFIFO == nullptr)
    {
        return;
    }

    while (toWifiFIFO->peekSize() > 0)
    {
        const uint16_t len = toWifiFIFO->peekSize();
        if (!client->canSend() || client->space() < len)
        {
            return;
        }

        toWifiFIFO->popSize();
        uint8_t data[len];
        toWifiFIFO->popBytes(data, len);
        client->write((const char *)data, len);
    }
}

void GenericTCPSocket::begin(ConnectionMode socketMode, uint16_t socketPort, IPAddress socketRemoteIP)
{
    instance = this;

    mode = socketMode;
    port = socketPort;
    remoteIP = socketRemoteIP;

    if (mode == TCP_SERVER)
    {
        tcpServer = new AsyncServer(port);
        tcpServer->onClient(handleNewClient, tcpServer);
        tcpServer->begin();
    }
    else // TCP_CLIENT
    {
        needToConnect = true;
    }
}

void GenericTCPSocket::setupClient()
{
    if (tcpClient != nullptr)
    {
        tcpClient->close(true);
        delete tcpClient;
    }

    tcpClient = new AsyncClient();
    if (tcpClient != nullptr)
    {
        tcpClient->onData(handleDataIn, NULL);
        tcpClient->onError(handleError, NULL);
        tcpClient->onDisconnect(handleDisconnect, NULL);
        tcpClient->onTimeout(handleTimeOut, NULL);
        tcpClient->setRxTimeout(clientTimeoutS);

        if (tcpClient->connect(remoteIP, port))
        {
            DBGLN("TCP Client connecting to %s:%u", remoteIP.toString().c_str(), port);
        }
        else
        {
            DBGLN("TCP Client failed to initiate connection to %s:%u", remoteIP.toString().c_str(), port);
        }
    }
}

void GenericTCPSocket::handle()
{
    if (mode == TCP_CLIENT && needToConnect)
    {
        setupClient();
        needToConnect = false;
    }

    flushOutgoingData();
}

void GenericTCPSocket::write(const uint8_t* data, size_t len)
{
    if (toWifiFIFO == nullptr)
    {
        toWifiFIFO = new FIFO<TCP_BUFFER_SIZE>();
    }

    if (!toWifiFIFO->available(len + 2))
    {
        DBGLN("TCP OUT: buffer full! wanted: %u, free: %u", (len + 2), toWifiFIFO->free());
        return;
    }

    toWifiFIFO->pushSize(len);
    toWifiFIFO->pushBytes(data, len);
    flushOutgoingData();
}

size_t GenericTCPSocket::read(uint8_t* buffer, size_t len)
{
    if (fromWifiFIFO == nullptr)
    {
        return 0;
    }

    if (fromWifiFIFO->peekSize() == 0)
    {
        return 0;
    }

    const uint16_t available = fromWifiFIFO->peekSize();
    if (available > len)
    {
        DBGLN("TCP IN: packet too large for read buffer! need: %u, have: %u", available, len);
        return 0;
    }

    fromWifiFIFO->popSize();
    fromWifiFIFO->popBytes(buffer, available);

    return available;
}

bool GenericTCPSocket::hasDataAvailable()
{
    if (fromWifiFIFO == nullptr)
    {
        return false;
    }

    return fromWifiFIFO->peekSize() > 0;
}

bool GenericTCPSocket::isConnected()
{
    if (mode == TCP_SERVER)
    {
        return (currentClient != nullptr && currentClient->connected());
    }
    else // TCP_CLIENT
    {
        return (tcpClient != nullptr && tcpClient->connected());
    }
}

bool GenericTCPSocket::needsToConnect()
{
    return needToConnect;
}

void GenericTCPSocket::handleDataIn(void *arg, AsyncClient *client, void *data, size_t len)
{
    if (instance->fromWifiFIFO == nullptr)
    {
        instance->fromWifiFIFO = new FIFO<TCP_BUFFER_SIZE>();
    }

    if (instance->fromWifiFIFO->available(len + 2)) // +2 because it takes 2 bytes to store the size of the FIFO chunk
    {
        instance->fromWifiFIFO->pushSize(len);
        instance->fromWifiFIFO->pushBytes((uint8_t *)data, len);
        DBGLN("TCP IN: queued %u bytes", len);
    }
    else
    {
        DBGLN("TCP IN: buffer full! wanted: %u, free: %u", (len + 2), instance->fromWifiFIFO->free());
    }
}

void GenericTCPSocket::handleError(void *arg, AsyncClient *client, int8_t error)
{
    DBGLN("\nclient %x connection error %s", client, client->errorToString(error));

    if (instance->mode == TCP_SERVER)
    {
        instance->clientDisconnect(client);
    }
    else // TCP_CLIENT
    {
        instance->needToConnect = true; // Retry connection
    }
}

void GenericTCPSocket::handleDisconnect(void *arg, AsyncClient *client)
{
    DBGLN("\nclient %x disconnected", client);

    if (instance->mode == TCP_SERVER)
    {
        instance->clientDisconnect(client);
    }
    else // TCP_CLIENT
    {
        instance->needToConnect = true; // Retry connection
    }
}

void GenericTCPSocket::handleTimeOut(void *arg, AsyncClient *client, uint32_t time)
{
    DBGLN("\nclient ACK timeout ip: %s", client->remoteIP().toString().c_str());

    if (instance->mode == TCP_SERVER)
    {
        instance->clientDisconnect(client);
    }
    else // TCP_CLIENT
    {
        instance->needToConnect = true; // Retry connection
    }
}

void GenericTCPSocket::clientConnect(AsyncClient *client)
{
    if (fromWifiFIFO == nullptr)
    {
        fromWifiFIFO = new FIFO<TCP_BUFFER_SIZE>();
    }

    if (currentClient != nullptr)
    {
        currentClient->close();
    }
    currentClient = client;
}

void GenericTCPSocket::clientDisconnect(AsyncClient *client)
{
    // NOTE: FIFO buffers stick around because the code doesn't account for multiple clients
    if (client == currentClient)
    {
        currentClient = nullptr;
    }
    client->close();
    delete client;
}

void GenericTCPSocket::handleNewClient(void *arg, AsyncClient *client)
{
    DBGLN("\nGenericTCPSocket client (%x) connected ip: %s", client, client->remoteIP().toString().c_str());

    instance->clientConnect(client);

    // register events
    client->onData(handleDataIn, NULL);
    client->onError(handleError, NULL);
    client->onDisconnect(handleDisconnect, NULL);
    client->onTimeout(handleTimeOut, NULL);
    client->setRxTimeout(clientTimeoutS);
}

#endif
