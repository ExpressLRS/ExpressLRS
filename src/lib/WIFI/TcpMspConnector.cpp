#if defined(TARGET_RX)

#include "TcpMspConnector.h"
#include "logging.h"

#include "CRSFRouter.h"
#include "crsf2msp.h"
#include "msp2crsf.h"

#define TCP_PORT_BETAFLIGHT     5761    // port 5761 as used by BF configurator for tcp://xxx connections
#define WS_ENDPOINT_MSP_URI     "/msp"  // URI path for BF configurator ws://xxx HTTP upgrade connections

#if defined(PLATFORM_ESP8266)
template <uint32_t FIFO_SIZE>
static bool queueFrame(FIFO<FIFO_SIZE> &queue, const uint8_t *data, const size_t len)
{
    if (len > MSP_FRAME_MAX_LEN)
    {
        return false;
    }
    const uint16_t required = len + sizeof(uint16_t);
    bool queued = false;
    queue.lock();
    while (!queue.available(required))
    {
        const uint16_t drop = queue.peekSize();
        if (drop == 0 || queue.size() < drop + sizeof(uint16_t))
        {
            queue.flush();
            break;
        }
        queue.skip(drop + sizeof(uint16_t));
    }
    if (queue.available(required))
    {
        queue.pushSize(len);
        queue.pushBytes(data, len);
        queued = true;
    }
    queue.unlock();
    return queued;
}
#endif

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

    WSserver = new AsyncWebSocket(WS_ENDPOINT_MSP_URI,
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
    mspPendingLen = 0;
    mspRequestInFlight = false;

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
#if defined(PLATFORM_ESP8266)
        wsQueue.flush();
#endif
        WSclient = client;
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        DBGLN("WS(%x) disconnected", client);
        if (client == WSclient)
        {
            WSclient = nullptr;
#if defined(PLATFORM_ESP8266)
            wsQueue.flush();
#endif
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
            handleInput(data, len);
        }
    }
}

void TcpMspConnector::flushWS()
{
    if (mspRequestInFlight && millis() - mspRequestStart > 1200)
    {
        mspPendingLen = 0;
        mspRequestInFlight = false;
    }
    processQueuedInput();

#if defined(PLATFORM_ESP8266)
    if (WSclient == nullptr || wsQueue.size() < sizeof(uint16_t) || WSclient->queueLen() != 0)
    {
        return;
    }

    uint16_t sendLen = 0;
    uint16_t dropLen = 0;
    while (wsQueue.size() >= dropLen + sizeof(uint16_t))
    {
        const uint16_t len = (uint16_t)wsQueue[dropLen] + ((uint16_t)wsQueue[dropLen + 1] << 8);
        if (len == 0 || wsQueue.size() < dropLen + len + sizeof(uint16_t))
        {
            wsQueue.flush();
            return;
        }
        if (sendLen + len > sizeof(wsFrame))
        {
            break;
        }
        for (uint16_t i = 0; i < len; ++i)
        {
            wsFrame[sendLen + i] = wsQueue[dropLen + sizeof(uint16_t) + i];
        }
        sendLen += len;
        dropLen += len + sizeof(uint16_t);
    }

    if (sendLen != 0 && WSclient->binary(wsFrame, sendLen))
    {
        wsQueue.skip(dropLen);
    }
#endif
}

void TcpMspConnector::handleInput(const uint8_t *data, const size_t len)
{
    if (len > MSP_FRAME_MAX_LEN)
    {
        return;
    }
    if (mspRequestInFlight)
    {
        memcpy(mspPendingFrame, data, len);
        mspPendingLen = len;
        return;
    }
    mspRequestInFlight = true;
    mspRequestStart = millis();
    msp2crsf->parse(this, data, len, CRSF_ADDRESS_BLUETOOTH_WIFI, CRSF_ADDRESS_FLIGHT_CONTROLLER);
}

void TcpMspConnector::processQueuedInput()
{
    if (mspRequestInFlight || mspPendingLen == 0)
    {
        return;
    }
    const uint16_t len = mspPendingLen;
    mspPendingLen = 0;
    handleInput(mspPendingFrame, len);
}

void TcpMspConnector::processData(AsyncClient *client, void *data, const size_t len)
{
    handleInput((uint8_t *)data, len);
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
#if defined(PLATFORM_ESP8266)
        if (WSclient)
        {
            if (WSclient->queueLen() == 0 && wsQueue.size() == 0)
            {
                if (!WSclient->binary(data, len))
                {
                    queueFrame(wsQueue, data, len);
                }
            }
            else
            {
                queueFrame(wsQueue, data, len);
            }
        }
#else
        if (WSclient)
            WSclient->binary(data, len);
#endif
        if (message->type == CRSF_FRAMETYPE_MSP_RESP)
        {
            mspRequestInFlight = false;
            processQueuedInput();
        }
    });
}

#endif