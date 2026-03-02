#pragma once

#include "targets.h"

#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)

#include <cstdint>
#include <cstring>
#include "ESPAsyncWebServer.h"
#include "FIFO.h"

#define TCP_BUFFER_SIZE 1024

enum ConnectionMode { TCP_SERVER, TCP_CLIENT };

// Generic TCP socket implementation that supports both server and client modes
class GenericTCPSocket
{
private:
    static GenericTCPSocket *instance;

    ConnectionMode mode;
    uint16_t port;
    IPAddress remoteIP;

    AsyncServer *tcpServer = nullptr;
    AsyncClient *currentClient = nullptr;
    AsyncClient *tcpClient = nullptr;
    static const uint32_t clientTimeoutS = 10U;
    bool needToConnect = true;

    AsyncClient *getActiveClient();
    void flushOutgoingData();

    static void handleNewClient(void *arg, AsyncClient *client);
    static void handleDataIn(void *arg, AsyncClient *client, void *data, size_t len);
    static void handleDisconnect(void *arg, AsyncClient *client);
    static void handleTimeOut(void *arg, AsyncClient *client, uint32_t time);
    static void handleError(void *arg, AsyncClient *client, int8_t error);

    void clientConnect(AsyncClient *client);
    void clientDisconnect(AsyncClient *client);
    void setupClient();

    FIFO<TCP_BUFFER_SIZE> *toWifiFIFO = nullptr;
    FIFO<TCP_BUFFER_SIZE> *fromWifiFIFO = nullptr;

public:
    void begin(ConnectionMode socketMode, uint16_t socketPort, IPAddress socketRemoteIP = INADDR_NONE);
    void handle();
    void write(const uint8_t* data, size_t len);
    size_t read(uint8_t* buffer, size_t len);
    bool hasDataAvailable();
    bool isConnected();
    bool needsToConnect();
};

#endif
