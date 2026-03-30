#include "SerialTCP.h"
#include "logging.h"

#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)

namespace {
SerialTCP *serialTcpOwner = nullptr;

SerialTCP *getSerialTcp()
{
    return serialTcpOwner;
}
}

SerialTCP::SerialTCP(Stream &output, Stream &input) : _outputPort(&output), _inputPort(&input)
{
}

SerialTCP::~SerialTCP()
{
    end();
}

void SerialTCP::begin(uint16_t port)
{
    end();

    tcpServer = new AsyncServer(port);
    tcpServer->onClient(handleNewClient, this);
    tcpServer->begin();
    tcpInitialized = true;
}

void SerialTCP::end()
{
    tcpInitialized = false;
    uartToTcpFifo.flush();
    tcpToUartFifo.flush();
    uartBatchPending = false;
    lastUartRxAtUs = 0;

    if (tcpClient != nullptr)
    {
        tcpClient->close(true);
        delete tcpClient;
        tcpClient = nullptr;
    }

    if (tcpServer != nullptr)
    {
        delete tcpServer;
        tcpServer = nullptr;
    }
}

void SerialTCP::handle()
{
    if (!tcpInitialized)
    {
        return;
    }

    flushTcpToUart(tcpBufferSize);
    handleUartInput();
    flushUartToTcp();
}

void SerialTCP::handleUartInput()
{
    if (_inputPort == nullptr)
    {
        return;
    }

    uint16_t remainingBudget = maxUartBytesPerHandle;
    while (remainingBudget > 0)
    {
        const size_t available = _inputPort->available();
        if (available == 0)
        {
            break;
        }

        const uint16_t bytesToRead = min<uint16_t>(min<size_t>(available, sizeof(tempBuffer)), remainingBudget);
        const size_t bytesRead = _inputPort->readBytes(tempBuffer, bytesToRead);
        if (bytesRead == 0)
        {
            break;
        }

        uartToTcpFifo.pushBytes(tempBuffer, bytesRead);
        uartBatchPending = true;
        lastUartRxAtUs = micros();

        if (uartToTcpFifo.size() >= uartFlushThreshold)
        {
            flushUartToTcp();
        }

        remainingBudget -= bytesRead;
    }
}

size_t SerialTCP::writeClient(AsyncClient *client, const uint8_t *data, size_t len)
{
    if (client == nullptr || !client->connected() || len == 0)
    {
        return 0;
    }

    if (!client->canSend())
    {
        return 0;
    }

    return client->write(reinterpret_cast<const char *>(data), len, ASYNC_WRITE_FLAG_COPY);
}

void SerialTCP::flushTcpToUart(uint32_t maxBytesToSend)
{
    uint32_t bytesWritten = 0;

    while (_outputPort != nullptr && tcpToUartFifo.size() > 0 && bytesWritten < maxBytesToSend)
    {
        const size_t writable = _outputPort->availableForWrite();
        if (writable == 0)
        {
            break;
        }

        const uint16_t bytesToWrite = min<uint16_t>(tcpToUartFifo.size(), min<uint32_t>(writable, maxBytesToSend - bytesWritten));
        if (bytesToWrite == 0)
        {
            break;
        }

        tcpToUartFifo.popBytes(tempBuffer, bytesToWrite);
        _outputPort->write(tempBuffer, bytesToWrite);
        bytesWritten += bytesToWrite;
    }
}

void SerialTCP::flushUartToTcp()
{
    if (tcpClient == nullptr || !tcpClient->connected())
    {
        uartToTcpFifo.flush();
        uartBatchPending = false;
        return;
    }

    if (uartToTcpFifo.size() == 0)
    {
        uartBatchPending = false;
        return;
    }

    if (uartBatchPending && uartToTcpFifo.size() < uartFlushThreshold)
    {
        const uint32_t nowUs = micros();
        if ((uint32_t)(nowUs - lastUartRxAtUs) < uartIdleFlushUs)
        {
            return;
        }
    }

    uartBatchPending = false;
    while (uartToTcpFifo.size() > 0)
    {
        const uint16_t bytesToWrite = min<uint16_t>(uartToTcpFifo.size(), sizeof(tempBuffer));
        uartToTcpFifo.popBytes(tempBuffer, bytesToWrite);
        const size_t written = writeClient(tcpClient, tempBuffer, bytesToWrite);
        if (written < bytesToWrite)
        {
            uartToTcpFifo.flush();
            uartToTcpFifo.pushBytes(tempBuffer + written, bytesToWrite - written);
            uartBatchPending = true;
            lastUartRxAtUs = micros();
            return;
        }
    }
}

void SerialTCP::clientConnect(AsyncClient *client)
{
    if (tcpClient != nullptr && tcpClient != client)
    {
        tcpClient->close();
    }

    uartToTcpFifo.flush();
    tcpToUartFifo.flush();
    uartBatchPending = false;
    lastUartRxAtUs = 0;

    tcpClient = client;
    client->setNoDelay(true);
    client->onData(handleDataIn, this);
    client->onError(handleError, this);
    client->onDisconnect(handleDisconnect, this);
    client->onTimeout(handleTimeOut, this);
    client->onAck(handleAck, this);
    client->onPoll(handlePoll, this);
    client->setRxTimeout(clientTimeoutS);
}

void SerialTCP::clientDisconnect(AsyncClient *client)
{
    uartToTcpFifo.flush();
    tcpToUartFifo.flush();
    uartBatchPending = false;
    lastUartRxAtUs = 0;

    if (client == tcpClient)
    {
        tcpClient = nullptr;
    }

    client->close();
    delete client;
}

void SerialTCP::handleNewClient(void *arg, AsyncClient *client)
{
    DBGLN("SerialTCP(%x) connected ip %s", client, client->remoteIP().toString().c_str());
    reinterpret_cast<SerialTCP *>(arg)->clientConnect(client);
}

void SerialTCP::handleDataIn(void *arg, AsyncClient *client, void *data, size_t len)
{
    auto *instance = reinterpret_cast<SerialTCP *>(arg);
    if (client != instance->tcpClient || instance->_outputPort == nullptr || len == 0)
    {
        return;
    }

    const auto *bytes = reinterpret_cast<const uint8_t *>(data);
    size_t written = 0;

    const size_t writable = instance->_outputPort->availableForWrite();
    if (writable > 0)
    {
        written = min(writable, len);
        instance->_outputPort->write(bytes, written);
    }

    if (written < len)
    {
        instance->tcpToUartFifo.pushBytes(bytes + written, len - written);
    }

    instance->flushTcpToUart(tcpBufferSize);
}

void SerialTCP::handleDisconnect(void *arg, AsyncClient *client)
{
    DBGLN("SerialTCP(%x) disconnected", client);
    reinterpret_cast<SerialTCP *>(arg)->clientDisconnect(client);
}

void SerialTCP::handleTimeOut(void *arg, AsyncClient *client, uint32_t time)
{
    (void)time;
    DBGLN("SerialTCP(%x) timeout", client);
    reinterpret_cast<SerialTCP *>(arg)->clientDisconnect(client);
}

void SerialTCP::handleError(void *arg, AsyncClient *client, err_t error)
{
    DBGLN("SerialTCP(%x) connection error %d", client, error);
    reinterpret_cast<SerialTCP *>(arg)->clientDisconnect(client);
}

void SerialTCP::handleAck(void *arg, AsyncClient *client, size_t len, uint32_t time)
{
    (void)client;
    (void)len;
    (void)time;
    reinterpret_cast<SerialTCP *>(arg)->flushUartToTcp();
}

void SerialTCP::handlePoll(void *arg, AsyncClient *client)
{
    (void)client;
    reinterpret_cast<SerialTCP *>(arg)->flushUartToTcp();
}

void serialTcpStart(Stream &output, Stream &input, uint16_t port)
{
    serialTcpStop();

    serialTcpOwner = new SerialTCP(output, input);
    serialTcpOwner->begin(port);
}

void serialTcpStop()
{
    if (serialTcpOwner == nullptr)
    {
        return;
    }

    serialTcpOwner->end();
    delete serialTcpOwner;
    serialTcpOwner = nullptr;
}

bool serialTcpIsActive()
{
    return getSerialTcp() != nullptr && getSerialTcp()->isActive();
}

static int start()
{
    return serialTcpIsActive() ? DURATION_IMMEDIATELY : DURATION_NEVER;
}

static int event()
{
    return serialTcpIsActive() ? DURATION_IMMEDIATELY : DURATION_NEVER;
}

static int timeout()
{
    SerialTCP *serialTcp = getSerialTcp();
    if (serialTcp == nullptr || !serialTcp->isActive())
    {
        return DURATION_NEVER;
    }

    serialTcp->handle();
    return DURATION_IMMEDIATELY;
}

device_t SerialTCP_device = {
    .initialize = nullptr,
    .start = start,
    .event = event,
    .timeout = timeout,
    .subscribe = EVENT_CONFIG_SERIAL_CHANGE | EVENT_CONNECTION_CHANGED
};

#endif
